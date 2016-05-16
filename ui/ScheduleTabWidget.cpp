/*
 *    This is QTournament, a badminton tournament management program.
 *    Copyright (C) 2014 - 2015  Volker Knollmann
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

#include <QMessageBox>

#include "ScheduleTabWidget.h"
#include "ui_ScheduleTabWidget.h"
#include "GuiHelpers.h"
#include "Score.h"
#include "ui/DlgMatchResult.h"
#include "ui/MainFrame.h"
#include "CentralSignalEmitter.h"
#include "MatchMngr.h"
#include "CatMngr.h"
#include "CourtMngr.h"
#include "ui/commonCommands/cmdCallMatch.h"

ScheduleTabWidget::ScheduleTabWidget(QWidget *parent) :
    QDialog(parent), db(nullptr),
    ui(new Ui::ScheduleTabWidget)
{
    ui->setupUi(this);

    // initialize sorting and filtering in the two match group views
    ui->mgIdleView->setFilter(MatchGroupTableView::FilterType::IDLE);
    ui->mgStagedView->setFilter(MatchGroupTableView::FilterType::STAGED);
    ui->mgStagedView->sortByColumn(MatchGroupTableModel::STAGE_SEQ_COL_ID, Qt::AscendingOrder);

    // react on selection changes in the IDLE match groups view
    connect(ui->mgIdleView->selectionModel(),
      SIGNAL(selectionChanged(const QItemSelection &, const QItemSelection &)),
      SLOT(onIdleSelectionChanged(const QItemSelection&, const QItemSelection&)));

    // react on selection changes in the STAGED match groups view
    connect(ui->mgStagedView->selectionModel(),
      SIGNAL(selectionChanged(const QItemSelection &, const QItemSelection &)),
      SLOT(onStagedSelectionChanged(const QItemSelection&, const QItemSelection&)));

    // react to changes in round,  match or category status
    CentralSignalEmitter* cse = CentralSignalEmitter::getInstance();
    connect(cse, SIGNAL(roundCompleted(int,int)), this, SLOT(onRoundCompleted(int,int)));
    connect(cse, SIGNAL(matchStatusChanged(int,int,OBJ_STATE,OBJ_STATE)), this, SLOT(onMatchStatusChanged(int,int,OBJ_STATE,OBJ_STATE)));
    connect(cse, SIGNAL(categoryStatusChanged(Category,OBJ_STATE,OBJ_STATE)), this, SLOT(onCatStatusChanged()));

    // default all buttons to "disabled"
    ui->btnSchedule->setEnabled(false);
    ui->btnStage->setEnabled(false);
    ui->btnUnstage->setEnabled(false);

    // initial status bar value
    QString rawProgressBarString = tr("No match status available");
    updateProgressBar();

    // create a timer for cyclic updates of the remaining tournament time
    progressbarUpdateTimer = make_unique<QTimer>(this);
    connect(progressbarUpdateTimer.get(), SIGNAL(timeout()), this, SLOT(updateProgressBar()));
    progressbarUpdateTimer->start(PROGRESSBAR_UPDATE_INTERVAL__SECS * 1000);
}

//----------------------------------------------------------------------------

ScheduleTabWidget::~ScheduleTabWidget()
{
  delete ui;
}

//----------------------------------------------------------------------------

void ScheduleTabWidget::setDatabase(TournamentDB* _db)
{
  db = _db;

  ui->mgIdleView->setDatabase(db);
  ui->mgStagedView->setDatabase(db);
  ui->tvCourts->setDatabase(db);
  ui->tvMatches->setDatabase(db);

  if (db != nullptr)
  {
    // things to do when we open a new tournament
    initProgressBarFromDatabase();
  } else {
    // things to do when we close a tournament

    // reset the progress bar
    ui->pbRemainingMatches->setValue(0);
    ui->pbRemainingMatches->setFormat(QString());
  }

  setEnabled(db != nullptr);
}

//----------------------------------------------------------------------------

void ScheduleTabWidget::updateRefereeColumn()
{
  ui->tvMatches->updateRefereeColumn();
}

//----------------------------------------------------------------------------

void ScheduleTabWidget::onBtnStageClicked()
{
  // lets check if a valid match group is selected
  auto mg = ui->mgIdleView->getSelectedMatchGroup();
  if (mg == nullptr) return;

  MatchMngr mm{db};
  if (mm.canStageMatchGroup(*mg) != OK) return;

  mm.stageMatchGroup(*mg);
}

//----------------------------------------------------------------------------

void ScheduleTabWidget::onBtnUnstageClicked()
{
  // lets check if a valid match group is selected
  auto mg = ui->mgStagedView->getSelectedMatchGroup();
  if (mg == nullptr) return;

  MatchMngr mm{db};
  if (mm.canUnstageMatchGroup(*mg) != OK) return;

  mm.unstageMatchGroup(*mg);
}

//----------------------------------------------------------------------------

void ScheduleTabWidget::onBtnScheduleClicked()
{
  // is at least one match group staged?
  MatchMngr mm{db};
  if (mm.getMaxStageSeqNum() == 0) return;

  mm.scheduleAllStagedMatchGroups();
  updateButtons();

  // resize columns to fit new matches best
  ui->tvMatches->horizontalHeader()->resizeSections(QHeaderView::ResizeToContents);
}

//----------------------------------------------------------------------------

void ScheduleTabWidget::onIdleSelectionChanged(const QItemSelection& selected, const QItemSelection& deselected)
{
  updateButtons();
}

//----------------------------------------------------------------------------

void ScheduleTabWidget::onStagedSelectionChanged(const QItemSelection& selected, const QItemSelection& deselected)
{
  updateButtons();
}

//----------------------------------------------------------------------------

void ScheduleTabWidget::updateButtons()
{
  MatchMngr mm{db};

  // update the "stage"-button
  auto mg = ui->mgIdleView->getSelectedMatchGroup();
  if ((mg != nullptr) && (mm.canStageMatchGroup(*mg) == OK))
  {
    ui->btnStage->setEnabled(true);
  } else {
    ui->btnStage->setEnabled(false);
  }

  // update the "unstage"-button
  mg = ui->mgStagedView->getSelectedMatchGroup();
  if ((mg != nullptr) && (mm.canUnstageMatchGroup(*mg) == OK))
  {
    ui->btnUnstage->setEnabled(true);
  } else {
    ui->btnUnstage->setEnabled(false);
  }

  // update the "Schedule"-button
  ui->btnSchedule->setEnabled(mm.getMaxStageSeqNum() != 0);
}

//----------------------------------------------------------------------------

void ScheduleTabWidget::onCourtDoubleClicked(const QModelIndex &index)
{
  auto court = ui->tvCourts->getSelectedCourt();
  if (court == nullptr) return;

  auto ma = court->getMatch();
  if (ma == nullptr)
  {
    return;  // no match assigned to this court
  }

  askAndStoreMatchResult(*ma);
}

//----------------------------------------------------------------------------

void ScheduleTabWidget::onRoundCompleted(int catId, int round)
{
  CatMngr cm{db};
  Category cat = cm.getCategoryById(catId);

  QString txt = tr("Round %1 of category %2 finished!").arg(round).arg(cat.getName());

  QMessageBox::information(this, tr("Round finished"), txt);
}

//----------------------------------------------------------------------------

void ScheduleTabWidget::askAndStoreMatchResult(const Match &ma)
{
  // only accept results for running matches
  if (ma.getState() != STAT_MA_RUNNING)
  {
    return;
  }

  // ask the user for the match result
  DlgMatchResult dlg(this, ma);
  dlg.setModal(true);
  int dlgResult = dlg.exec();
  if (dlgResult != QDialog::Accepted)
  {
    return;
  }
  auto matchResult = dlg.getMatchScore();
  assert(matchResult != nullptr);

  // create a (rather ugly) confirmation message box
  QString msg = tr("Please confirm:\n\n");
  msg += ma.getPlayerPair1().getDisplayName() + "\n";
  msg += "\tvs.\n";
  msg += ma.getPlayerPair2().getDisplayName() + "\n\n";
  msg += tr("Result: ");
  QString sResult = matchResult->toString();
  sResult = sResult.replace(",", ", ");
  msg += sResult + "\n\n\n";
  if (matchResult->getWinner() == 1)
  {
    msg += tr("WINNER: ");
    msg += ma.getPlayerPair1().getDisplayName();
    msg += "\n\n";
    msg += tr("LOSER: ");
    msg += ma.getPlayerPair2().getDisplayName();
  }
  if (matchResult->getWinner() == 2)
  {
    msg += tr("WINNER: ");
    msg += ma.getPlayerPair2().getDisplayName();
    msg += "\n\n";
    msg += tr("LOSER: ");
    msg += ma.getPlayerPair1().getDisplayName();
  }
  if (matchResult->getWinner() == 0)
  {
    msg += tr("The match result is DRAW");
  }
  msg += "\n\n";

  int confirm = QMessageBox::question(this, tr("Please confirm match result"), msg);
  if (confirm != QMessageBox::Yes) return;

  // actually store the data and update the internal object states
  MatchMngr mm{db};
  mm.setMatchScoreAndFinalizeMatch(ma, *matchResult);

  // ask the user if the next available match should be started on the
  // now free court
  //
  // first of all, check if there is a next match available
  int nextMatchId;
  int nextCourtId;
  ERR e = mm.getNextViableMatchCourtPair(&nextMatchId, &nextCourtId, true);
  if ((e == NO_MATCH_AVAIL) || (nextMatchId < 1))
  {
    return;
  }

  // now ask if the match should be started
  confirm = QMessageBox::question(this, tr("Next Match"), tr("Start the next available match on the free court?"));
  if (confirm != QMessageBox::Yes) return;

  // instead of the court determined by getNextViableMatchCourtPair we use
  // the court of the previous match
  auto oldCourt = ma.getCourt();
  assert(oldCourt != nullptr);

  // try to start the match on the old court
  auto nextMatch = mm.getMatch(nextMatchId);
  assert(nextMatch != nullptr);
  cmdCallMatch cmd{this, *nextMatch, *oldCourt};
  cmd.exec();
}

//----------------------------------------------------------------------------

int ScheduleTabWidget::estimateRemainingTournamentTime()
{
  // we can't do any estimations if don't have courts
  // to play on
  CourtMngr cm{db};
  int courtCount = cm.getActiveCourtCount();
  if (courtCount <= 0) return -1;

  QDateTime curDateTime = QDateTime::currentDateTimeUtc();
  uint epochNow = curDateTime.toTime_t();

  // calculate the average runtime of all running matches
  int totalRuntime = 0;
  int totalRuntimeCnt = 0;
  MatchMngr mm{db};
  auto runningMatches = mm.getCurrentlyRunningMatches();
  for (const Match& ma : runningMatches)
  {
    auto startTime = ma.getStartTime();
    if (startTime.isNull()) continue;

    uint startTimeSecs = startTime.toTime_t();

    // calculate the time the match is already running
    int runtime = epochNow - startTimeSecs;

    // sum everything up
    totalRuntime += runtime;
    ++totalRuntimeCnt;
  }

  // calculate the overall remaining playing time for all unfinished matches
  int nTotal;
  int nFinished;
  int nRunning;
  tie(nTotal, nFinished, nRunning) = mm.getMatchStats();
  int remainingMatches = nTotal - nFinished;
  int avgMatchDuration = getAverageMatchDuration();
  int remainingTime = (remainingMatches / courtCount) * avgMatchDuration;
  remainingTime += ((remainingMatches % courtCount) == 0) ? 0 : avgMatchDuration;

  // subtract the time already "consumed" by the running matches (averaged)
  remainingTime -= (totalRuntimeCnt > 0) ? totalRuntime / totalRuntimeCnt : 0;

  return remainingTime;
}

//----------------------------------------------------------------------------

int ScheduleTabWidget::getAverageMatchDuration()
{
  // calculate the total match duration of all finished matches
  totalDuration = 0;
  totalDurationCnt = 0;
  MatchMngr mm{db};
  for (const Match& ma : mm.getFinishedMatches())
  {
    int duration = ma.getMatchDuration();
    if (duration < 0) continue;
    totalDuration += duration;
    ++totalDurationCnt;
  }

  return (totalDurationCnt > 0) ? totalDuration / totalDurationCnt : INITIAL_AVG_MATCH_DURATION__SECS;
}

//----------------------------------------------------------------------------

void ScheduleTabWidget::updateProgressBar()
{
  if (db == nullptr)
  {
    ui->pbRemainingMatches->setFormat(QString());
    ui->pbRemainingMatches->setValue(0);
    return;
  }

  QString txt = rawProgressBarString;
  int timeRemain = estimateRemainingTournamentTime();
  if (timeRemain < 0)
  {
    txt = txt.arg(tr("<unknown>"));
  } else {
    int hours = timeRemain / 3600;
    int minutes = (timeRemain % 3600) / 60;
    QString s = "%1 h %2 min";
    s = s.arg(hours);
    s = s.arg(minutes, 2, 10, QLatin1Char('0'));
    txt = txt.arg(s);
  }
  ui->pbRemainingMatches->setFormat(txt);
}

//----------------------------------------------------------------------------

void ScheduleTabWidget::initProgressBarFromDatabase()
{
  // calculate the total match duration of all finished matches
  totalDuration = 0;
  totalDurationCnt = 0;
  MatchMngr mm{db};
  for (const Match& ma : mm.getFinishedMatches())
  {
    int duration = ma.getMatchDuration();
    if (duration < 0) continue;
    totalDuration += duration;
    ++totalDurationCnt;
  }

  // update the progress bar using dummy match parameters
  onMatchStatusChanged(-1, -1, STAT_MA_FINISHED, STAT_MA_FINISHED);
}

//----------------------------------------------------------------------------

void ScheduleTabWidget::onMatchStatusChanged(int matchId, int matchSeqNum, OBJ_STATE fromState, OBJ_STATE toState)
{
  // get updated match status counters
  MatchMngr mm{db};
  int nTotal;
  int nFinished;
  int nRunning;
  tie(nTotal, nFinished, nRunning) = mm.getMatchStats();
  int percComplete = (nTotal > 0) ? (nFinished * 100) / nTotal : 0;

  // update the match duration counter, if applicable
  /*if ((toState == STAT_MA_FINISHED) && (matchId > 0))
  {
    auto ma = tnmt->getMatchMngr()->getMatch(matchId);
    assert(ma != nullptr);
    int duration = ma->getMatchDuration();

    if (duration >= 0)
    {
      totalDuration += duration;
      ++totalDurationCnt;
    }
  }*/

  // put everything into a status string
  rawProgressBarString = tr("%1 matches in total, %2 finished (%3 %), %5 unfinished, %4 running");
  rawProgressBarString = rawProgressBarString.arg(nTotal).arg(nFinished).arg(percComplete).arg(nRunning).arg(nTotal - nFinished);
  rawProgressBarString += "   " + tr("Avg. match duration: %1 min., est. remaining tournament time: %2");
  rawProgressBarString = rawProgressBarString.arg(getAverageMatchDuration() / 60);

  // update the progress bar
  ui->pbRemainingMatches->setValue(percComplete);
  updateProgressBar();
}

//----------------------------------------------------------------------------

void ScheduleTabWidget::onCatStatusChanged()
{
  // update the progress bar using dummy match parameters
  onMatchStatusChanged(-1, -1, STAT_MA_FINISHED, STAT_MA_FINISHED);
}

//----------------------------------------------------------------------------


//----------------------------------------------------------------------------


//----------------------------------------------------------------------------


//----------------------------------------------------------------------------


//----------------------------------------------------------------------------


//----------------------------------------------------------------------------


//----------------------------------------------------------------------------

