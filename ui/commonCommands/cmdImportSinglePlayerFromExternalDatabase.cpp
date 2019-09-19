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

#include <QObject>
#include <QMessageBox>

#include "cmdImportSinglePlayerFromExternalDatabase.h"
#include "ui/DlgImportPlayer.h"
#include "ui/DlgPickPlayerSex.h"
#include "ui/dlgEditPlayer.h"
#include "cmdCreatePlayerFromDialog.h"
#include "PlayerMngr.h"
#include "CatMngr.h"

using namespace QTournament;

cmdImportSinglePlayerFromExternalDatabase::cmdImportSinglePlayerFromExternalDatabase(const TournamentDB& _db, QWidget* p, int _preselectedCatId)
  :AbstractCommand(_db, p), preselectedCatId(_preselectedCatId)
{

}

//----------------------------------------------------------------------------

Error cmdImportSinglePlayerFromExternalDatabase::exec()
{
  // make sure we have an external database open
  PlayerMngr pm{db};
  if (!(pm.hasExternalPlayerDatabaseAvailable()))
  {
    QString msg = tr("No valid database for player export available.\n\n");
    msg +=tr("Is the database configured and the file existing?");
    QMessageBox::warning(parentWidget, tr("Export player"), msg);
    return Error::EPD_NotOpened;
  }
  Error e = pm.openConfiguredExternalPlayerDatabase();
  if (!(pm.hasExternalPlayerDatabaseOpen()) || (e != Error::OK))
  {
    QString msg = tr("Could not open database for player export!");
    QMessageBox::warning(parentWidget, tr("Export player"), msg);
    return Error::EPD_NotOpened;
  }

  ExternalPlayerDB* extDb = pm.getExternalPlayerDatabaseHandle();

  // show a search-and-select dialog
  DlgImportPlayer dlg{parentWidget, extDb};
  if (dlg.exec() != QDialog::Accepted)
  {
    return Error::OK;
  }

  // get the selected ID
  int extId = dlg.getSelectedExternalPlayerId();
  if (extId < 0)
  {
    QString msg = tr("No valid player selection found");
    QMessageBox::warning(parentWidget, tr("Import player"), msg);
    return Error::InvalidId;
  }

  // get the selected player from the database
  auto extPlayer = extDb->getPlayer(extId);
  if (!extPlayer)
  {
    QString msg = tr("No valid player selection found");
    QMessageBox::warning(parentWidget, tr("Import player"), msg);
    return Error::InvalidId;
  }

  // if the player has no valid sex assigned,
  // show a selection dialog
  std::optional<ExternalPlayerDatabaseEntry> finalPlayerData;
  if (extPlayer->getSex() == Sex::DontCare)
  {
    DlgPickPlayerSex dlgPickSex{parentWidget, extPlayer->getFirstname() + " " + extPlayer->getLastname()};
    if (dlgPickSex.exec() != QDialog::Accepted)
    {
      return Error::OK;
    }

    finalPlayerData.emplace(
          extPlayer->getFirstname(),
          extPlayer->getLastname(),
          dlgPickSex.getSelectedSex()
          );
  } else {
    finalPlayerData = extPlayer;
  }

  // make sure the player's sex fits to a possibly preselected
  // category
  if (preselectedCatId > 0)
  {
    CatMngr cm{db};
    auto cat = cm.getCategory(preselectedCatId);

    // was the provided ID valid? If not, invalidate it
    if (!cat) preselectedCatId = -1;

    // may we add a player of the selected sex to this category?
    Sex selSex = finalPlayerData->getSex();
    if (cat->getAddState(selSex) != CatAddState::CanJoin)
    {
      QString msg = tr("%1 cannot be added to this category.");
      msg = msg.arg((selSex == Sex::M) ? tr("A male player") : tr("A female player"));
      QMessageBox::warning(parentWidget, tr("Import player"), msg);
      return Error::InvalidSex;
    }
  }


  // now we have all player details, so we can prepare
  // the insert-player-dialog to allow the user the
  // selection of a team and possible category assignments
  DlgEditPlayer dlgCreate{db, parentWidget, *finalPlayerData, preselectedCatId};

  // let an external command do the rest of the work
  cmdCreatePlayerFromDialog cmd{db, parentWidget, &dlgCreate};
  return cmd.exec();
}

