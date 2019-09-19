/*
 *    This is QTournament, a badminton tournament management program.
 *    Copyright (C) 2014 - 2017  Volker Knollmann
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

#include "cmdUnregisterPlayer.h"
#include "PlayerMngr.h"

using namespace QTournament;

cmdUnregisterPlayer::cmdUnregisterPlayer(QWidget* p, const Player& _pl)
  :AbstractCommand(_pl.getDatabaseHandle(), p), pl(_pl)
{

}

//----------------------------------------------------------------------------

Error cmdUnregisterPlayer::exec()
{
  // set the "wait for registration"-flag
  Error err;
  PlayerMngr pm{db};

  err = pm.setWaitForRegistration(pl, true);

  if (err == Error::OK) return Error::OK; // no error

  QString msg = tr("The player is already assigned to matches\n");
  msg += tr("and/or currently running categories.\n\n");
  msg += tr("Can't undo the player registration.");
  QMessageBox::warning(parentWidget, tr("Player unregister failed"), msg);

  return err;
}

