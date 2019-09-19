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

#include "TeamTabWidget.h"

#include "MainFrame.h"
#include "TeamMngr.h"

using namespace QTournament;

TeamTabWidget::TeamTabWidget()
:QWidget()

{
  ui.setupUi(this);
}

//----------------------------------------------------------------------------

TeamTabWidget::~TeamTabWidget()
{
}

//----------------------------------------------------------------------------

void TeamTabWidget::setDatabase(const TournamentDB* _db)
{
  db = _db;

  ui.teamList->setDatabase(db);

  setEnabled(db != nullptr);
}

//----------------------------------------------------------------------------

void TeamTabWidget::onCreateTeamClicked()
{
  int cnt = 0;
  
  // try to create new teams using a
  // canonical name until it finally succeeds
  Error e = Error::NameExists;
  while (e != Error::OK)
  {
    QString teamName = tr("New Team ") + QString::number(cnt);
    
    TeamMngr tm{*db};
    e = tm.createNewTeam(teamName);
    cnt++;
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

