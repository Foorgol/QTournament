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

#include "Team.h"
#include "TournamentDataDefs.h"
#include "TournamentDB.h"
#include "TournamentErrorCodes.h"
#include "TeamMngr.h"

using namespace SqliteOverlay;

namespace QTournament
{

  Team::Team(const TournamentDB& _db, int rowId)
  :TournamentDatabaseObject(_db, TabTeam, rowId)
  {
  }

//----------------------------------------------------------------------------

  Team::Team(const TournamentDB& _db, const SqliteOverlay::TabRow& _row)
  :TournamentDatabaseObject(_db, _row)
  {
  }

//----------------------------------------------------------------------------

  QString Team::getName(int maxLen) const
  {
    QString fullName = QString::fromUtf8(row[GenericNameFieldName].data());
    
    if (maxLen < 1)
    {
      return fullName;
    }
    
    if (maxLen == 1)
    {
      return fullName.left(1);
    }
    
    if (maxLen == 2)
    {
      return fullName.left(1) + ".";
    }
    
    if ((maxLen > 2) && (maxLen < fullName.length()))
    {
      return fullName.left(maxLen - 1) + ".";
    }
    
    return fullName;
  }

//----------------------------------------------------------------------------

  Error Team::rename(const QString& nn)
  {
    TeamMngr tm{db};
    return tm.renameTeam(*this, nn);
  }

//----------------------------------------------------------------------------

  int Team::getMemberCount() const
  {
    DbTab playerTab = DbTab{db.get(), TabPlayer, false};
    return playerTab.getMatchCountForColumnValue(PL_TeamRef, getId());
  }

//----------------------------------------------------------------------------

  int Team::getUnregisteredMemberCount() const
  {
    WhereClause wc;
    wc.addCol(PL_TeamRef, getId());
    wc.addCol(GenericStateFieldName, static_cast<int>(ObjState::PL_WaitForRegistration));
    DbTab playerTab = DbTab{db.get(), TabPlayer, false};
    return playerTab.getMatchCountForWhereClause(wc);
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

}
