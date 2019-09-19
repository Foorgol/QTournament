/*
 *    This is QTournament, a badminton tournament management program.
 *    Copyright (C) 2014 - 2019  Volker Knollmann
 *
 *    This program is free software: you can redistribute it and/or modify
 *    it under the terms of the GNU General Public License as published by
 *    the Free Software Foundation, either version 3 of the License, or
 *    any later version.
 *
 *    This program is distributed in the hope that it will be useful,
 *    but WITHOUT ANY WARRANTY; without even the implied warranty of
 *    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *    GNU General Public License for more details.
 *
 *    You should have received a copy of the GNU General Public License
 *    along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <stdexcept>
#include <iostream>

#include <QMessageBox>
#include <QFileDialog>
#include <QFile>
#include <QTime>
#include <QPushButton>

#include "MainFrame.h"
#include "MatchMngr.h"
#include "CourtMngr.h"
#include "PlayerMngr.h"
#include "TeamMngr.h"
#include "CatMngr.h"
#include "ui/DlgTournamentSettings.h"
#include "CourtMngr.h"
#include "OnlineMngr.h"
#include "DlgPassword.h"
#include "commonCommands/cmdOnlineRegistration.h"
#include "commonCommands/cmdSetOrChangePassword.h"
#include "commonCommands/cmdStartOnlineSession.h"
#include "commonCommands/cmdDeleteFromServer.h"
#include "commonCommands/cmdConnectionSettings.h"
#include "HelperFunc.h"
#include "BuiltinTestScenarios.h"

using namespace QTournament;

//----------------------------------------------------------------------------

MainFrame::MainFrame()
{
  ui.setupUi(this);
  showMaximized();

  // no database file is initially active
  currentDatabaseFileName.clear();

  // prepare an action to toggle the test-menu's visibility
  scToggleTestMenuVisibility = new QShortcut(QKeySequence(Qt::CTRL + Qt::ALT + Qt::Key_T), this);
  scToggleTestMenuVisibility->setContext(Qt::ApplicationShortcut);
  connect(scToggleTestMenuVisibility, SIGNAL(activated()), this, SLOT(onToggleTestMenuVisibility()));
  isTestMenuVisible = true;
  onToggleTestMenuVisibility();

  // initialize timers for polling the database's dirty flag
  // and for triggering the autosave function
  dirtyFlagPollTimer = make_unique<QTimer>(this);
  connect(dirtyFlagPollTimer.get(), SIGNAL(timeout()), this, SLOT(onDirtyFlagPollTimerElapsed()));
  dirtyFlagPollTimer->start(DirtyFlagPollIntervall_ms);
  autosaveTimer = make_unique<QTimer>(this);
  connect(autosaveTimer.get(), SIGNAL(timeout()), this, SLOT(onAutosaveTimerElapsed()));
  autosaveTimer->start(AutosaveIntervall_ms);

  // prepare a status bar label that shows the last autosave time
  lastAutosaveTimeStatusLabel = new QLabel(statusBar());
  lastAutosaveTimeStatusLabel->clear();
  statusBar()->addPermanentWidget(lastAutosaveTimeStatusLabel);

  // prepare a timer and label that show the current sync state
  syncStatLabel = new QLabel(statusBar());
  syncStatLabel->clear();
  statusBar()->addWidget(syncStatLabel);
  serverSyncTimer = make_unique<QTimer>(this);
  connect(serverSyncTimer.get(), SIGNAL(timeout()), this, SLOT(onServerSyncTimerElapsed()));
  serverSyncTimer->start(ServerSyncStatusInterval_ms);

  // prepare a button for triggering a server ping test
  btnPingTest = new QPushButton(statusBar());
  btnPingTest->setText(tr("Ping"));
  statusBar()->addWidget(btnPingTest);
  connect(btnPingTest, SIGNAL(clicked(bool)), this, SLOT(onBtnPingTestClicked()));
  btnPingTest->setEnabled(false);


  // finally disable all widgets by setting their database instance to nullptr
  distributeCurrentDatabasePointerToWidgets();
  enableControls(false);

}

//----------------------------------------------------------------------------

MainFrame::~MainFrame()
{
}

//----------------------------------------------------------------------------

void MainFrame::newTournament()
{
  // show a dialog for setting the tournament parameters
  DlgTournamentSettings dlg{this};
  dlg.setModal(true);
  int result = dlg.exec();

  if (result != QDialog::Accepted)
  {
    return;
  }

  // make sure we get the settings from the dialog
  auto settings = dlg.getTournamentSettings();
  if (!settings)
  {
    QMessageBox::warning(this, tr("New tournament"), tr("Something went wrong; no new tournament created."));
    return;
  }

  // close any open tournament
  if (currentDb != nullptr)
  {
    bool isOkay = closeCurrentTournament();
    if (!isOkay) return;
  }

  auto newDb = make_unique<TournamentDB>(":memory:", *settings);
  if (!newDb)
  {
    // shouldn't happen, because the file has been
    // deleted before (see above)
    QMessageBox::warning(this, tr("New tournament"), tr("Something went wrong; no new tournament created."));
    return;
  }

  // make the new database the current one
  currentDb = std::move(newDb);
  distributeCurrentDatabasePointerToWidgets();

  // create the initial number of courts
  CourtMngr cm{*currentDb};
  for (int i=0; i < dlg.getCourtCount(); ++i)
  {
    cm.createNewCourt(i+1, QString::number(i+1));
    // no error checking here. Creation at this point must always succeed.
  }

  // prepare for autosaving
  currentDb->resetDirtyFlag();
  onAutosaveTimerElapsed();
  enableControls(true);
  updateWindowTitle();
}

//----------------------------------------------------------------------------

void MainFrame::openTournament()
{
  // ask for the file name
  QFileDialog fDlg{this};
  fDlg.setAcceptMode(QFileDialog::AcceptOpen);
  fDlg.setFileMode(QFileDialog::ExistingFile);
  fDlg.setNameFilter(tr("QTournament Files (*.tdb)"));
  int result = fDlg.exec();

  if (result != QDialog::Accepted)
  {
    return;
  }

  // get the filename
  QString filename = fDlg.selectedFiles().at(0);

  // try to open the tournament file DIRECTLY
  //
  // we operate only temporarily directly on the file
  // until possible format conversions etc. are completed.
  std::unique_ptr<TournamentDB> newDb{nullptr};
  try
  {
    newDb = make_unique<TournamentDB>(QString2StdString(filename));
  }
  catch (TournamentException&)
  {
    QString msg = tr("The file has been created with an incompatible\n");
    msg += tr("version of QTournament and can't be opened.");
    QMessageBox::critical(this, tr("Open tournament"), msg);
    return;
  }
  catch (std::invalid_argument&)
  {
    QString msg = tr("Couldn't open ") + filename;
    QMessageBox::critical(this, tr("Open tournament"), msg);
    return;
  }
  if (!newDb) // shouldn't happen after the previous checks
  {
    QMessageBox::warning(this, tr("Open tournament"), tr("Something went wrong; no tournament opened."));
    return;
  }

  // do we need to convert this database to a new format?
  if (newDb->needsConversion())
  {
    QString msg = tr("The file has been created with an older version of QTournament.\n\n");
    msg += tr("The file is not compatible with the current version but it can be updated ");
    msg += tr("to the current version. If you upgrade, you will no longer be able to open ");
    msg += tr("the file with older versions of QTournament.\n\n");
    msg += tr("The conversion cannot be undone.\n\n");
    msg += tr("Do you want to proceed and update the file?");
    int result = QMessageBox::question(this, tr("Convert file format?"), msg);
    if (result != QMessageBox::Yes) return;

    bool conversionOk = newDb->convertToLatestDatabaseVersion();
    if (conversionOk)
    {
      msg = tr("The file was successfully converted!");
      QMessageBox::information(this, tr("Convert file format"), msg);
    } else {
      msg = tr("The file conversion failed. The tournament could not be opened.");
      QMessageBox::critical(this, tr("Convert file format"), msg);
      return;
    }
  }

  // close any open tournament
  if (currentDb != nullptr)
  {
    bool isOkay = closeCurrentTournament();
    if (!isOkay) return;
  }

  // close the temporarily opened tournament database and
  // re-open it as a copy in memory
  try
  {
    newDb = make_unique<TournamentDB>();
    newDb->restoreFromFile(QString2StdString(filename));
  }
  catch (std::invalid_argument&)
  {
    QString msg;
    msg = tr("Could not read from the source file:\n\n");
    msg += filename + "\n\n";
    msg += tr("The tournament has not been opened.");

    QMessageBox::warning(this, tr("Opening failed"), msg);
    return;
  }
  catch (SqliteOverlay::GenericSqliteException& ex)
  {
    QString msg;
    msg = tr("A database error occured while opening.\n\n");
    msg += tr("Internal hint: SQLite error code = %1");
    msg = msg.arg(static_cast<int>(ex.errCode()));

    QMessageBox::warning(this, tr("Opening failed"), msg);
    return;
  }

  // opening was successfull ==> distribute the database handle to all widgets
  QApplication::setOverrideCursor(QCursor(Qt::WaitCursor));
  currentDb = std::move(newDb);
  distributeCurrentDatabasePointerToWidgets();
  enableControls(true);
  QApplication::restoreOverrideCursor();
  currentDatabaseFileName = filename;
  ui.actionCreate_baseline->setEnabled(true);
  currentDb->resetDirtyFlag();
  onAutosaveTimerElapsed();

  // BAAAD HACK: when the OnlineManager instance was created, the config table
  // was still empty because instanciation takes place before
  // restoreFromFile() is triggered.
  // Thus, the OnlineManager does not read custom server settings, if
  // existent. We need to trigger this manually here.
  //
  // Remember: in this application it is bad, bad, bad to keep state in the
  // xManagers... or somewhere else
  currentDb->getOnlineManager()->applyCustomServerSettings();

  // show the tournament name in the main window's title
  updateWindowTitle();

  // open the external player database file, if configured
  PlayerMngr pm(*currentDb);
  if (pm.hasExternalPlayerDatabaseConfigured())
  {
    Error err = pm.openConfiguredExternalPlayerDatabase();

    if (err == Error::OK) return;

    QString msg;
    switch (err)
    {
    case Error::EPD_NotFound:
      msg = tr("Could not find the player database\n\n");
      msg += pm.getExternalDatabaseName() + "\n\n";
      msg += tr("Please make sure the file exists and is valid.");
      break;

    default:
      msg = tr("The player database\n\n");
      msg += pm.getExternalDatabaseName() + "\n\n";
      msg += tr("is invalid.");
    }

    QMessageBox::warning(this, "Open player database", msg);
  }
}

//----------------------------------------------------------------------------

void MainFrame::onSave()
{
  if (currentDb == nullptr) return;

  execCmdSave();
}

//----------------------------------------------------------------------------

void MainFrame::onSaveAs()
{
  if (currentDb == nullptr) return;

  execCmdSaveAs();
}

//----------------------------------------------------------------------------

void MainFrame::onSaveCopy()
{
  if (currentDb == nullptr) return;

  QString dstFileName = askForTournamentFileName(tr("Save a copy"));
  if (dstFileName.isEmpty()) return;

  bool isOkay = saveCurrentDatabaseToFile(dstFileName, false, true);
  if (isOkay)
  {
    onAutosaveTimerElapsed();  // update the status line
  }
}

//----------------------------------------------------------------------------

void MainFrame::onCreateBaseline()
{
  if (currentDb == nullptr) return;
  if (currentDatabaseFileName.isEmpty()) return;

  // determine the basename of the current database file
  if (!(currentDatabaseFileName.endsWith(".tdb", Qt::CaseInsensitive)))
  {
    QString msg = tr("The current file name is unexpectedly ugly. Can't derive\n");
    msg += tr("baseline names from it.\n\n");
    msg += tr("Nothing has been saved.");
    QMessageBox::warning(this, tr("Baseline creation failed"), msg);
    return;
  }
  QString basename = currentDatabaseFileName.left(currentDatabaseFileName.length() - 4);

  // determine a suitable name for the baseline
  QString dstName;
  int cnt = 0;
  bool nameFound = false;
  while (!nameFound)
  {
    dstName = basename + "__%1.tdb";
    dstName = dstName.arg(cnt, 4, 10, QLatin1Char{'0'});
    nameFound = !(QFile::exists(dstName));
    ++cnt;
  }

  bool isOkay = saveCurrentDatabaseToFile(dstName, false, true);
  if (isOkay)
  {
    onAutosaveTimerElapsed();

    QString msg = tr("A snapshot of the current tournament status has been saved to:\n\n%1");
    msg = msg.arg(dstName);
    QMessageBox::information(this, "Create baseline", msg);
  }
}

//----------------------------------------------------------------------------

void MainFrame::onClose()
{
  if (currentDb == nullptr) return;

  if (closeCurrentTournament()) enableControls(false);
}

//----------------------------------------------------------------------------

void MainFrame::enableControls(bool doEnable)
{
  ui.centralwidget->setEnabled(doEnable);
  ui.actionSettings->setEnabled(doEnable);
  ui.menuExternal_player_database->setEnabled(doEnable);
  ui.actionSave->setEnabled(doEnable);
  ui.actionSave_as->setEnabled(doEnable);
  ui.actionSave_a_copy->setEnabled(doEnable);
  ui.actionCreate_baseline->setEnabled(doEnable && !(currentDatabaseFileName.isEmpty()));
  ui.actionClose->setEnabled(doEnable);

  ui.menuOnline->setEnabled(doEnable);
  if (doEnable)
  {
    updateOnlineMenu();
  }

  btnPingTest->setEnabled(doEnable);
}

//----------------------------------------------------------------------------

bool MainFrame::closeCurrentTournament()
{
  // close other possibly open tournaments
  if (currentDb != nullptr)
  {
    if (currentDb->isDirty())
    {
      QString msg = tr("Warning: all unsaved changes to the current tournament\n");
      msg += tr("will be lost.\n\n");
      msg += tr("Do you want to save your changes?");

      QMessageBox msgBox{this};
      msgBox.setText(msg);
      msgBox.setStandardButtons(QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel);
      msgBox.setWindowTitle(tr("Save changes?"));
      msgBox.setIcon(QMessageBox::Warning);
      int result = msgBox.exec();

      if (result == QMessageBox::Cancel) return false;

      if (result == QMessageBox::Save)
      {
        bool isOkay = execCmdSave();
        if (!isOkay) return false;
      }
    }

    //
    // At this point, the user either decided to discard all changes
    // or the database has been saved successfully
    //
    // ==> we can shut the current database down
    //

    // disconnect from the external player database, if any
    PlayerMngr pm{*currentDb};
    pm.closeExternalPlayerDatabase();

    // force all widgets to forget the database handle
    // BEFORE we actually close the database
    distributeCurrentDatabasePointerToWidgets(true);

    // close the database
    currentDb->close();
    currentDb.reset();
    currentDatabaseFileName.clear();
    onAutosaveTimerElapsed();
  }
  
  // delete the test file, if existing
  QString testFileName = QDir().absoluteFilePath("tournamentTestFile.tdb");
  if (QFile::exists(testFileName))
  {
    QFile::remove(testFileName);
  }
  
  onAutosaveTimerElapsed();
  enableControls(false);
  updateWindowTitle();

  return true;
}

//----------------------------------------------------------------------------

void MainFrame::distributeCurrentDatabasePointerToWidgets(bool forceNullptr)
{
  TournamentDB* db = forceNullptr ? nullptr : currentDb.get();

  ui.tabPlayers->setDatabase(db);
  ui.tabCategories->setDatabase(db);
  ui.tabTeams->setDatabase(db);
  ui.tabSchedule->setDatabase(db);
  ui.tabReports->setDatabase(db);
  ui.tabMatchLog->setDatabase(db);
}

//----------------------------------------------------------------------------

bool MainFrame::saveCurrentDatabaseToFile(const QString& dstFileName, bool resetDirtyFlagOnSuccess, bool showErrorOnFailure)
{
  // Precondition:
  // All checks for valid filenames, overwriting of files etc. have to
  // be done before calling this function.
  //
  // This function unconditionally writes to the destination file, whether
  // it exists or not.

  if (currentDb == nullptr) return false;

  // write the database to the file
  try
  {
    bool isOkay = currentDb->backupToFile(QString2StdString(dstFileName));

    if (isOkay && resetDirtyFlagOnSuccess)
    {
      currentDb->resetDirtyFlag();
      currentDb->resetLocalChangeCounter();
    }

    return isOkay;
  }
  catch (SqliteOverlay::BusyException&)
  {
    if (showErrorOnFailure)
    {
      QString msg;
      msg = tr("Could not write to %1 because the file is locked by some other application.\n\n");
      msg += tr("The tournament has not been saved.");
      msg = msg.arg(dstFileName);

      QMessageBox::warning(this, tr("Saving failed"), msg);
    }
    return false;
  }
  catch (SqliteOverlay::GenericSqliteException& ex)
  {
    if (showErrorOnFailure)
    {
      QString msg;

      msg = tr("A database error occured while saving.\n\n");
      msg += tr("Internal hint: SQLite error code = %1");
      msg = msg.arg(static_cast<int>(ex.errCode()));

      QMessageBox::warning(this, tr("Saving failed"), msg);
    }
    return false;
  }
}

//----------------------------------------------------------------------------

bool MainFrame::execCmdSave()
{
  if (currentDb == nullptr) return false;

  if (currentDatabaseFileName.isEmpty())
  {
    return execCmdSaveAs();
  }

  bool isOkay = saveCurrentDatabaseToFile(currentDatabaseFileName, true, true);
  if (isOkay)
  {
    onAutosaveTimerElapsed();
  }

  return isOkay;
}

//----------------------------------------------------------------------------

bool MainFrame::execCmdSaveAs()
{
  if (currentDb == nullptr) return false;

  QString dstFileName = askForTournamentFileName(tr("Save tournament as"));
  if (dstFileName.isEmpty()) return false;  // user abort counts as "failed"

  bool isOkay = saveCurrentDatabaseToFile(dstFileName, true, true);

  if (isOkay)
  {
    currentDatabaseFileName = dstFileName;
    ui.actionCreate_baseline->setEnabled(true);

    onAutosaveTimerElapsed();

    // show the file name in the window title
    updateWindowTitle();
  }

  return isOkay;
}

//----------------------------------------------------------------------------

QString MainFrame::askForTournamentFileName(const QString& dlgTitle)
{
  // ask for the file name
  QFileDialog fDlg{this};
  fDlg.setAcceptMode(QFileDialog::AcceptSave);
  fDlg.setNameFilter(tr("QTournament Files (*.tdb)"));
  fDlg.setWindowTitle(dlgTitle);
  int result = fDlg.exec();

  if (result != QDialog::Accepted)
  {
    return "";
  }

  // get the filename and fix the extension, if necessary
  QString filename = fDlg.selectedFiles().at(0);
  QString ext = filename.right(4).toLower();
  if (ext != ".tdb") filename += ".tdb";

  return filename;
}

//----------------------------------------------------------------------------

void MainFrame::updateWindowTitle()
{
  QString title = "QTournament";

  if (currentDb != nullptr)
  {
    // determine the tournament title
    SqliteOverlay::KeyValueTab cfg{*currentDb, TabCfg};
    QString tnmtTitle = stdString2QString(cfg[CfgKey_TnmtName]);
    title += " - " + tnmtTitle + " (%1)";

    // insert the current filename, if any
    title = currentDatabaseFileName.isEmpty() ? title.arg(tr("unsaved")) : title.arg(currentDatabaseFileName);

    // append an asterisk to the windows title if the
    // database has changed since the last saving
    if (currentDb->isDirty()) title += " *";
  }

  setWindowTitle(title);
}

//----------------------------------------------------------------------------

void MainFrame::updateOnlineMenu()
{
  if (currentDb == nullptr)
  {
    ui.menuOnline->setEnabled(false);
    return;
  }

  OnlineMngr* om = currentDb->getOnlineManager();
  bool hasReg = om->hasRegistrationSubmitted();
  SyncState st = om->getSyncState();


  // disable registration if we've already registered once
  ui.actionRegister->setEnabled(!hasReg);

  // if we've not registered yet, there's no point in
  // setting / changing the password
  // ==> online enable the password item for registered tournaments
  ui.actionSet_Change_Password->setEnabled(hasReg);

  // deletion of the tournament only if we've registered before
  ui.actionDelete_from_Server->setEnabled(hasReg);

  // connect only if disconnected
  ui.actionConnect->setEnabled(hasReg && !(st.hasSession()));

  // disconnect only if connected
  ui.actionDisconnect->setEnabled(hasReg && st.hasSession());

  // change server settings only when disconnected
  ui.actionConnection_Settings->setEnabled(!(st.hasSession()));
}

//----------------------------------------------------------------------------

void MainFrame::setupTestScenario(int scenarioID)
{
  if ((scenarioID < 0) || (scenarioID > 8))
  {
    QMessageBox::critical(this, "Setup Test Scenario", "The scenario ID " + QString::number(scenarioID) + " is invalid!");
    return;
  }

  // shutdown whatever is open right now
  closeCurrentTournament();

  QString testFileName = QDir().absoluteFilePath("tournamentTestFile.tdb");

  // prepare a brand-new scenario
  TournamentSettings cfg;
  cfg.organizingClub = "SV Whatever";
  cfg.tournamentName = "World Championship";
  cfg.useTeams = true;
  cfg.refereeMode = RefereeMode::None;
  currentDb = std::make_unique<TournamentDB>(QString2StdString(testFileName), cfg);
  if (!currentDb)
  {
    return;
  }

  /*SqliteOverlay::DbTab t{*db, "Team", false};
  SqliteOverlay::ColumnValueClause cvc;
  cvc.addCol("Name","FakeTeam");
  cvc.addCol("SequenceNumber", 0);
  t.insertRow(cvc);*/

  // activate the fresh scenario including all GUI elements,
  // signals, slots, delegates, ...
  distributeCurrentDatabasePointerToWidgets();

  /*t = SqliteOverlay::DbTab{*currentDb, "Team", false};
  cvc.clear();
  cvc.addCol("Name","FakeTeam2");
  cvc.addCol("SequenceNumber", 1);
  t.insertRow(cvc);*/

  // actually run the fake scenario
  BuiltinScenarios::setupTestScenario(*currentDb, scenarioID);
  enableControls(true);

}

//----------------------------------------------------------------------------

void MainFrame::setupEmptyScenario()
{
  setupTestScenario(0);
}

//----------------------------------------------------------------------------

void MainFrame::setupScenario01()
{
  setupTestScenario(1);
}

//----------------------------------------------------------------------------

void MainFrame::setupScenario02()
{
  setupTestScenario(2);
}

//----------------------------------------------------------------------------

void MainFrame::setupScenario03()
{
  setupTestScenario(3);
}

//----------------------------------------------------------------------------

void MainFrame::setupScenario04()
{
  setupTestScenario(4);
}

//----------------------------------------------------------------------------

void MainFrame::setupScenario05()
{
  setupTestScenario(5);
}

//----------------------------------------------------------------------------

void MainFrame::setupScenario06()
{
  setupTestScenario(6);
}

//----------------------------------------------------------------------------

void MainFrame::setupScenario07()
{
  setupTestScenario(7);
}

//----------------------------------------------------------------------------

void MainFrame::setupScenario08()
{
  setupTestScenario(8);
}

//----------------------------------------------------------------------------

void MainFrame::closeEvent(QCloseEvent* ev)
{
  if (currentDb == nullptr)
  {
    ev->accept();
  } else {
    bool isOkay = closeCurrentTournament();

    if (!isOkay) ev->ignore();
    else ev->accept();
  }
}

//----------------------------------------------------------------------------

void MainFrame::onCurrentTabChanged(int newCurrentTab)
{
  if (newCurrentTab < 0) return;

  // get the newly selected tab widget
  auto selectedTabWidget = ui.mainTab->currentWidget();
  if (selectedTabWidget == nullptr) return;

  // check if the new tab is the reports tab
  if (selectedTabWidget == ui.tabReports)
  {
    ui.tabReports->onReloadRequested();
  }
}

//----------------------------------------------------------------------------

void MainFrame::onNewExternalPlayerDatabase()
{
  // ask for the file name
  QFileDialog fDlg{this};
  fDlg.setAcceptMode(QFileDialog::AcceptSave);
  fDlg.setNameFilter(tr("QTournament Player Database (*.pdb)"));
  int result = fDlg.exec();

  if (result != QDialog::Accepted)
  {
    return;
  }

  // get the filename and fix the extension, if necessary
  QString filename = fDlg.selectedFiles().at(0);
  QString ext = filename.right(4).toLower();
  if (ext != ".pdb") filename += ".pdb";

  // if the file exists, delete it.
  // the user has consented to the deletion in the
  // dialog
  if (QFile::exists(filename))
  {
    bool removeResult =  QFile::remove(filename);

    if (!removeResult)
    {
      QMessageBox::warning(this, tr("New player database"), tr("Could not delete ") + filename + tr(", no new database created."));
      return;
    }
  }

  // actually create and actiate the new database
  PlayerMngr pm{*currentDb};
  Error e = pm.setExternalPlayerDatabase(filename, true);
  if (e != Error::OK)
  {
    QMessageBox::warning(this, tr("New player database"), tr("Could not create ") + filename);
    return;
  }
}

//----------------------------------------------------------------------------

void MainFrame::onSelectExternalPlayerDatabase()
{
  // ask for the file name
  QFileDialog fDlg{this};
  fDlg.setAcceptMode(QFileDialog::AcceptOpen);
  fDlg.setFileMode(QFileDialog::ExistingFile);
  fDlg.setNameFilter(tr("QTournament Player Database (*.pdb)"));
  int result = fDlg.exec();

  if (result != QDialog::Accepted)
  {
    return;
  }

  // get the filename
  QString filename = fDlg.selectedFiles().at(0);

  // open and activate the database
  PlayerMngr pm{*currentDb};
  Error e = pm.setExternalPlayerDatabase(filename, false);
  if (e != Error::OK)
  {
    QString msg = tr("Could not open ") + filename + "\n\n";
    if (pm.hasExternalPlayerDatabaseOpen())
    {
      msg += "Database not changed.";
    } else  {
      msg += "No player database active.";
    }
    QMessageBox::warning(this, tr("Select player database"), msg);
    return;
  }
}

//----------------------------------------------------------------------------

void MainFrame::onInfoMenuTriggered()
{
  QString msg = tr("This is QTournament version %1.<br>");
  msg += tr("© Volker Knollmann, 2014 - 2019<br><br>");
  msg += tr("This program is free software: you can redistribute it and/or modify ");
  msg += tr("it under the terms of the GNU General Public License as published by ");
  msg += tr("the Free Software Foundation, either version 3 of the License, or ");
  msg += tr("any later version.<br>");

  msg += tr("This program is distributed in the hope that it will be useful, ");
  msg += tr("but WITHOUT ANY WARRANTY; without even the implied warranty of ");
  msg += tr("MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the ");
  msg += tr("GNU General Public License for more details.<br>");

  msg += tr("You should have received a copy of the GNU General Public License ");
  msg += tr("along with this program. If not, see <a href='http://www.gnu.org/licenses/'>http://www.gnu.org/licenses/</a>.<br><br>");

  msg += tr("The source code for this program is hosted on Github:<br>");
  msg += "<a href='https://github.com/Foorgol/QTournament'>https://github.com/Foorgol/QTournament</a><br><br>";

  msg += tr("For more information please visit:<br>");
  msg += "<a href='http://tournament.de'>http://tournament.de</a> (German)<br>";
  msg += "<a href='http://tournament.de/en'>http://tournament.de/en</a> (English)<br><br>";
  msg += "or send an email to <a href='mailto:info@qtournament.de'>info@qtournament.de</a>";
  msg = msg.arg(PRG_VERSION_STRING);

  QMessageBox msgBox{this};
  msgBox.setWindowTitle(tr("About QTournament"));
  msgBox.setTextFormat(Qt::RichText);
  msgBox.setText(msg);
  msgBox.exec();
}

//----------------------------------------------------------------------------

void MainFrame::onEditTournamentSettings()
{
  if (currentDb == nullptr) return;

  // show a dialog for setting the tournament parameters
  DlgTournamentSettings dlg{*currentDb, this};
  dlg.setModal(true);
  int result = dlg.exec();

  if (result != QDialog::Accepted)
  {
    return;
  }

  // get the new settings
  auto newSettings = dlg.getTournamentSettings();
  if (!newSettings)
  {
    QMessageBox::warning(this, tr("Edit tournament settings"),
                         tr("The tournament settings could not be updated."));
    return;
  }

  // check for changes and apply them.
  //
  // start with the tournament organizer
  SqliteOverlay::KeyValueTab cfg{*currentDb, TabCfg};
  QString oldTnmtOrga = stdString2QString(cfg[CfgKey_TnmtOrga]);
  if (oldTnmtOrga != newSettings->organizingClub)
  {
    cfg.set(CfgKey_TnmtOrga, QString2StdString(newSettings->organizingClub));
  }

  // the tournament name
  QString oldTnmtName = stdString2QString(cfg[CfgKey_TnmtName]);
  if (oldTnmtName != newSettings->tournamentName)
  {
    cfg.set(CfgKey_TnmtName, QString2StdString(newSettings->tournamentName));

    // refresh the window title to show the new name
    updateWindowTitle();
  }

  // the umpire mode
  RefereeMode oldRefereeMode = static_cast<RefereeMode>(cfg.getInt(CfgKey_DefaultRefereemode));
  if (oldRefereeMode != newSettings->refereeMode)
  {
    cfg.set(CfgKey_DefaultRefereemode, static_cast<int>(newSettings->refereeMode));
    ui.tabSchedule->updateRefereeColumn();
  }
}

//----------------------------------------------------------------------------

void MainFrame::onSetPassword()
{
  if (currentDb == nullptr) return;

  cmdSetOrChangePassword cmd{this, *currentDb};
  cmd.exec();
  updateOnlineMenu();
}

//----------------------------------------------------------------------------

void MainFrame::onRegisterTournament()
{
  if (currentDb == nullptr) return;

  // the online registration is a complex task
  // so I've moved it to a separate file

  cmdOnlineRegistration cmd{this, *currentDb};
  cmd.exec();
  updateOnlineMenu();
}

//----------------------------------------------------------------------------

void MainFrame::onStartSession()
{
  if (currentDb == nullptr) return;

  cmdStartOnlineSession cmd{this, *currentDb};
  cmd.exec();
  updateOnlineMenu();
}

//----------------------------------------------------------------------------

void MainFrame::onTerminateSession()
{
  OnlineMngr* om = currentDb->getOnlineManager();
  bool isOkay = om->disconnect();

  if (isOkay)
  {
    QMessageBox::information(this, tr("Server Disconnect"), tr("You are now disconnected from the server"));
  } else {
    QString msg = tr("An error occurred while disconnecting. Nevertheless that, the syncing with the server has now been stopped.");
    QMessageBox::warning(this, tr("Server Disconnect"), msg);
  }

  updateOnlineMenu();
}

//----------------------------------------------------------------------------

void MainFrame::onDeleteFromServer()
{
  if (currentDb == nullptr) return;

  cmdDeleteFromServer cmd{this, *currentDb};
  cmd.exec();
  updateOnlineMenu();
}

//----------------------------------------------------------------------------

void MainFrame::onEditConnectionSettings()
{
  if (currentDb == nullptr) return;

  cmdConnectionSetting cmd{this, *currentDb};
  cmd.exec();
  updateOnlineMenu();
}

//----------------------------------------------------------------------------

void MainFrame::onToggleTestMenuVisibility()
{
  ui.menubar->clear();
  ui.menubar->addMenu(ui.menuTournament);
  ui.menubar->addMenu(ui.menuOnline);
  ui.menubar->addMenu(ui.menuAbout_QTournament);

  if (!isTestMenuVisible)
  {
    ui.menubar->addMenu(ui.menuTesting);
  }

  isTestMenuVisible = !isTestMenuVisible;
}

//----------------------------------------------------------------------------

void MainFrame::onDirtyFlagPollTimerElapsed()
{
  static bool lastDirtyState{false};

  if (currentDb == nullptr) return;

  if (currentDb->isDirty() != lastDirtyState)
  {
    lastDirtyState = !lastDirtyState;
    updateWindowTitle();
  }
}

//----------------------------------------------------------------------------

void MainFrame::onAutosaveTimerElapsed()
{
  static int lastAutosaveDirtyCounterValue{0};

  if (currentDb == nullptr)
  {
    lastAutosaveTimeStatusLabel->clear();
    return;
  }

  if (!(currentDb->isDirty()))
  {
    lastAutosaveTimeStatusLabel->clear();
    lastAutosaveDirtyCounterValue = 0;
    return;
  }

  QString msg = tr("Last autosave: ");
  if (currentDatabaseFileName.isEmpty())
  {
    lastAutosaveTimeStatusLabel->setText(msg + tr("not yet possible"));
    return;
  }

  // do we need an autosave?
  if (currentDb->getLocalChangeCounter_total() > lastAutosaveDirtyCounterValue)
  {
    QString fname = currentDatabaseFileName + ".autosave";
    bool isOkay = saveCurrentDatabaseToFile(fname, false, false);

    if (isOkay)
    {
      msg += QTime::currentTime().toString("HH:mm:ss");
      lastAutosaveDirtyCounterValue = currentDb->getLocalChangeCounter_total();
    } else {
      msg += tr("failed");
    }
    lastAutosaveTimeStatusLabel->setText(msg);
  }
}

//----------------------------------------------------------------------------

void MainFrame::onServerSyncTimerElapsed()
{
  if (currentDb == nullptr)
  {
    syncStatLabel->clear();
    btnPingTest->setVisible(false);
    return;
  }

  // retrieve the status from the online manager
  OnlineMngr* om = currentDb->getOnlineManager();

  // show nothing if we've never registered the tournament
  if (!(om->hasRegistrationSubmitted()))
  {
    syncStatLabel->clear();
    btnPingTest->setVisible(false);
    return;
  } else {
    btnPingTest->setVisible(true);
  }

  // get the current sync state
  SyncState st = om->getSyncState();

  // if we're offline, everything's easy
  if (!(st.hasSession()))
  {
    syncStatLabel->setText(tr("<span style='color: red; font-weight: bold;'>Offline</span>"));
    return;
  }

  // we're online, thus we'll first update the labels
  // with the current status
  QString msg = tr("<span style='color: green; font-weight: bold;'>Online</span>");
  msg += tr(", %1 syncs committed, %2 changes pending");
  msg = msg.arg(st.partialSyncCounter);
  msg = msg.arg(currentDb->getChangeLogLength());

  // attach the last request time, if available
  int dt = om->getLastReqTime_ms();
  if (dt > 0)
  {
    msg += tr(" ; the last request took %1 ms");
    msg = msg.arg(dt);
  }

  // set the label and we're done with the cosmetics
  syncStatLabel->setText(msg);

  // next we check if the OnlineMngr wants to
  // do a sync call
  if (!(om->wantsToSync())) return;

  //
  // yes, a sync is necessary
  //

  QString errMsgFromServer;
  QApplication::setOverrideCursor(QCursor(Qt::WaitCursor));
  OnlineError err = om->doPartialSync(errMsgFromServer);
  QApplication::restoreOverrideCursor();

  // maybe the database is locked by a different process,
  // e.g. an open dialog
  if (err == OnlineError::LocalDatabaseBusy) return; // try again later

  // handle connection / transport errors
  msg.clear();
  if ((err != OnlineError::Okay) && (err != OnlineError::TransportOkay_AppError))
  {
    switch (err)
    {
    case OnlineError::Timeout:
      msg = tr("The server is currently not available.\n\n");
      msg += tr("Maybe the server is temporarily down or you are offline.");
      break;

    case OnlineError::BadRequest:
      msg = tr("The server did not accept our sync request (400, BadRequest).");
      break;

    default:
      msg = tr("Sync failed due to an unspecified network or server error!");
    }
  }

  if (err == OnlineError::TransportOkay_AppError)
  {
    if (errMsgFromServer == "DatabaseError")
    {
      msg = tr("Syncing failed because of a server-side database error.");
    }
    if (errMsgFromServer == "CSVError")
    {
      msg = tr("Syncing failed because the server couldn't digest our CSV data!\n");
      msg += tr("Strange, this shouldn't happen...");
    }
    if (msg.isEmpty())
    {
      msg = tr("Sync failed because of an unexpected server error.\n");
    }
  }

  if (!(msg.isEmpty()))
  {
    om->disconnect();
    msg += "\n\nThe server connection has been shut-down. Try to connect again later. Good luck!";
    QMessageBox::warning(this, tr("Server sync failed"), msg);
  }
}

//----------------------------------------------------------------------------

void MainFrame::onBtnPingTestClicked()
{
  if (currentDb == nullptr) return;
  OnlineMngr* om = currentDb->getOnlineManager();

  QApplication::setOverrideCursor(QCursor(Qt::WaitCursor));
  int t = om->ping();
  QApplication::restoreOverrideCursor();

  QString msg;
  if (t > 0)
  {
    msg = tr("The server responded within %1 ms");
    msg = msg.arg(t);
    QMessageBox::information(this, tr("Server Ping Test"), msg);
  } else {
    msg = tr("The server is not reachable");
    QMessageBox::warning(this, tr("Server Ping Test"), msg);
  }
}

//----------------------------------------------------------------------------


//----------------------------------------------------------------------------


//----------------------------------------------------------------------------


//----------------------------------------------------------------------------


//----------------------------------------------------------------------------


//----------------------------------------------------------------------------


//----------------------------------------------------------------------------


//----------------------------------------------------------------------------


//----------------------------------------------------------------------------


//----------------------------------------------------------------------------


//----------------------------------------------------------------------------


//----------------------------------------------------------------------------


//----------------------------------------------------------------------------


//----------------------------------------------------------------------------


//----------------------------------------------------------------------------


//----------------------------------------------------------------------------


//----------------------------------------------------------------------------


//----------------------------------------------------------------------------


//----------------------------------------------------------------------------


//----------------------------------------------------------------------------


//----------------------------------------------------------------------------


//----------------------------------------------------------------------------


//----------------------------------------------------------------------------


//----------------------------------------------------------------------------


//----------------------------------------------------------------------------


//----------------------------------------------------------------------------


