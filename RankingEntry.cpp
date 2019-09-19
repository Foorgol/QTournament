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

#include <stdexcept>

#include <SqliteOverlay/ClausesAndQueries.h>

#include "RankingEntry.h"
#include "TournamentDataDefs.h"
#include "TournamentDB.h"
#include "TournamentErrorCodes.h"
#include "CatMngr.h"

namespace QTournament
{

  RankingEntry::RankingEntry(const TournamentDB& _db, int rowId)
  :TournamentDatabaseObject(_db, TabMatchSystem, rowId)
  {
  }

//----------------------------------------------------------------------------

  RankingEntry::RankingEntry(const TournamentDB& _db, const SqliteOverlay::TabRow& _row)
  :TournamentDatabaseObject(_db, _row)
  {
  }

//----------------------------------------------------------------------------

  int RankingEntry::getRound() const
  {
    return row.getInt(RA_Round);
  }

//----------------------------------------------------------------------------

  Category RankingEntry::getCategory() const
  {
    int catId = row.getInt(RA_CatRef);
    CatMngr cm{db};
    return cm.getCategoryById(catId);
  }

//----------------------------------------------------------------------------

  std::optional<PlayerPair> RankingEntry::getPlayerPair() const
  {
    auto ppId = row.getInt2(RA_PairRef);
    if (!ppId.has_value()) return std::optional<PlayerPair>{};

    return PlayerPair{db, *ppId};
  }

//----------------------------------------------------------------------------

  int RankingEntry::getRank() const
  {
    int rank = row.getInt2(RA_Rank).value_or(-1);
    if (rank <= 0) return NoRankAssigned;

    return rank;
  }

//----------------------------------------------------------------------------

  int RankingEntry::getGroupNumber() const
  {
    return row.getInt(RA_GrpNum);
  }

//----------------------------------------------------------------------------

  std::tuple<int, int, int, int> RankingEntry::getMatchStats() const
  {
    int won = row.getInt2(RA_MatchesWon).value_or(0);
    int draw = row.getInt2(RA_MatchesDraw).value_or(0);
    int lost = row.getInt2(RA_MatchesLost).value_or(0);

    return std::tuple{won, draw, lost, won+draw+lost};
  }

//----------------------------------------------------------------------------

  std::tuple<int, int, int> RankingEntry::getGameStats() const
  {
    int won = row.getInt2(RA_GamesWon).value_or(0);
    int lost = row.getInt2(RA_GamesLost).value_or(0);

    return std::tuple{won, lost, won+lost};
  }

//----------------------------------------------------------------------------

  std::tuple<int, int> RankingEntry::getPointStats() const
  {
    int won = row.getInt2(RA_PointsWon).value_or(0);
    int lost = row.getInt2(RA_PointsLost).value_or(0);

    return std::tuple{won, lost};
  }

//----------------------------------------------------------------------------

//----------------------------------------------------------------------------


//----------------------------------------------------------------------------

}
