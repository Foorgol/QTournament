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

#ifndef TEAMMNGR_H
#define	TEAMMNGR_H

#include "TournamentDB.h"
#include "Team.h"
#include "Player.h"
#include "TournamentDataDefs.h"
#include "TournamentErrorCodes.h"
#include "DbTab.h"
#include "GenericObjectManager.h"

#include <QList>

using namespace dbOverlay;

namespace QTournament
{

  class TeamMngr : public QObject, GenericObjectManager
  {
    Q_OBJECT
  
  public:
    TeamMngr (TournamentDB* _db);
    ERR createNewTeam (const QString& teamName);
    bool hasTeam (const QString& teamName);
    Team getTeam (const QString& name);
    QList<Team> getAllTeams ();
    ERR renameTeam (Team& t, const QString& nn);
    Team getTeamBySeqNum (int seqNum);
    Team getTeamById (int id);
    ERR changeTeamAssigment(const Player& p, const Team& newTeam);

    PlayerList getPlayersForTeam(const Team& t) const;

    static std::function<bool (Team&, Team&)> getTeamSortFunction_byName();

  private:
    DbTab teamTab;

signals:
    void beginCreateTeam ();
    void endCreateTeam (int newTeamSeqNum);
    void teamRenamed(int teamSeqNum);
    void teamAssignmentChanged(const Player& affectedPlayer, const Team& oldTeam, const Team& newTeam);

  };
}

#endif	/* TEAMMNGR_H */

