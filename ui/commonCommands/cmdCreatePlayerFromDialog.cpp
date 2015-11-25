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

#include <QObject>
#include <QMessageBox>

#include "Tournament.h"
#include "cmdCreatePlayerFromDialog.h"

cmdCreatePlayerFromDialog::cmdCreatePlayerFromDialog(QWidget* p, DlgEditPlayer* initializedDialog)
  :AbstractCommand(p), dlg(initializedDialog)
{

}

//----------------------------------------------------------------------------

ERR cmdCreatePlayerFromDialog::exec()
{
  if (dlg->exec() != QDialog::Accepted)
  {
    return OK;
  }

  // we can be sure that all selected data in the dialog
  // is valid. That has been checked before the dialog
  // returns with "Accept". So we can directly step
  // into the creation of the new player
  ERR e = Tournament::getPlayerMngr()->createNewPlayer(
                                                       dlg->getFirstName(),
                                                       dlg->getLastName(),
                                                       dlg->getSex(),
                                                       dlg->getTeam().getName()
                                                       );

  if (e != OK)
  {
    QString msg = tr("Something went wrong when inserting the player. This shouldn't happen.");
    msg += tr("For the records: error code = ") + QString::number(static_cast<int>(e));
    QMessageBox::warning(parentWidget, tr("WTF??"), msg);
    return e;
  }
  Player p = Tournament::getPlayerMngr()->getPlayer(dlg->getFirstName(), dlg->getLastName());

  // assign the player to the selected categories
  //
  // we can be sure that all selected categories in the dialog
  // are valid. That has been checked upon creation of the "selectable"
  // category entries. So we can directly step
  // into the assignment of the categories
  CatMngr* cmngr = Tournament::getCatMngr();

  QHash<Category, bool> catSelection = dlg->getCategoryCheckState();
  QHash<Category, bool>::const_iterator it = catSelection.constBegin();

  while (it != catSelection.constEnd()) {
    if (it.value()) {
      Category cat = it.key();
      ERR e = cmngr->addPlayerToCategory(p, cat);

      if (e != OK) {
        QString msg = tr("Something went wrong when adding the player to a category. This shouldn't happen.");
        msg += tr("For the records: error code = ") + QString::number(static_cast<int> (e));
        QMessageBox::warning(parentWidget, tr("WTF??"), msg);
      }
    }
    ++it;
  }

  return OK;
}

