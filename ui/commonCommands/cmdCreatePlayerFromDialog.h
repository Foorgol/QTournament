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

#ifndef CMDCREATEPLAYERFROMDIALOG_H
#define CMDCREATEPLAYERFROMDIALOG_H

#include <QObject>

#include "AbstractCommand.h"
#include "Player.h"
#include "../dlgEditPlayer.h"


class cmdCreatePlayerFromDialog : public QObject, AbstractCommand
{
  Q_OBJECT

public:
  cmdCreatePlayerFromDialog(const QTournament::TournamentDB& _db, QWidget* p, DlgEditPlayer* initializedDialog);
  virtual QTournament::Error exec() override;
  virtual ~cmdCreatePlayerFromDialog() {}

protected:
  DlgEditPlayer* dlg;
};

#endif // CMDCREATEPLAYERFROMDIALOG_H
