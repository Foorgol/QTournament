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

#include <vector>

#include "PlayerProfile.h"
#include "Player.h"
#include "PlayerMngr.h"
#include "PlayerPair.h"
#include "Match.h"
#include "MatchMngr.h"

using namespace SqliteOverlay;

namespace QTournament
{

  PlayerProfile::PlayerProfile(const Player& _p)
    :db{_p.getDatabaseHandle()}, p{_p}, lastPlayedMatchId{-1},
      currentMatchId{-1}, nextMatchId{-1}, lastUmpireMatchId{-1},
      currentUmpireMatchId{-1}, nextUmpireMatchId{-1},
      finishCount{0}, walkoverCount{0}, scheduledCount{0},
      umpireFinishedCount{0}
  {
    initMatchLists();
    initMatchIds();
  }

  //----------------------------------------------------------------------------

  std::optional<Match> PlayerProfile::getLastPlayedMatch() const
  {
    return returnMatchOrEmpty(lastPlayedMatchId);
  }

  //----------------------------------------------------------------------------

  std::optional<Match> PlayerProfile::getCurrentMatch() const
  {
    return returnMatchOrEmpty(currentMatchId);
  }

  //----------------------------------------------------------------------------

  std::optional<Match> PlayerProfile::getNextMatch() const
  {
    return returnMatchOrEmpty(nextMatchId);
  }

  //----------------------------------------------------------------------------

  std::optional<Match> PlayerProfile::getLastUmpireMatch() const
  {
    return returnMatchOrEmpty(lastUmpireMatchId);
  }

  //----------------------------------------------------------------------------

  std::optional<Match> PlayerProfile::getCurrentUmpireMatch() const
  {
    return returnMatchOrEmpty(currentUmpireMatchId);
  }

  //----------------------------------------------------------------------------

  std::optional<Match> PlayerProfile::getNextUmpireMatch() const
  {
    return returnMatchOrEmpty(nextUmpireMatchId);
  }

  //----------------------------------------------------------------------------

  void PlayerProfile::initMatchIds()
  {
    MatchMngr mm{db};

    // loop over all umpire matches and find the next scheduled match,
    // the currently running match and the last finished match
    QDateTime lastFinishTime;
    int nextMatchNum = -1;
    for (const Match& ma : matchesAsUmpire)
    {
      ObjState stat = ma.getState();

      if (stat == ObjState::MA_Finished) ++umpireFinishedCount;

      if (ma.isInState(ObjState::MA_Running))
      {
        currentUmpireMatchId = ma.getId();
        continue;
      }

      if (stat == ObjState::MA_Finished)
      {
        QDateTime fTime = ma.getFinishTime();
        if (fTime.isValid())
        {
          if ((!(lastFinishTime.isValid())) || (fTime > lastFinishTime))  // this does not include walkovers
          {
            lastFinishTime = fTime;
            lastUmpireMatchId = ma.getId();
            continue;
          }
        }
      }

      int maNum = ma.getMatchNumber();
      if ((maNum != MatchNumNotAssigned) && ((maNum < nextMatchNum) || (nextMatchNum < 0)))
      {
        nextMatchNum = maNum;
        nextUmpireMatchId = ma.getId();
      }
    }


    // loop over all player matches and find the next scheduled match,
    // the currently running match and the last finished match
    lastFinishTime = QDateTime{};   // set to "invalid"
    nextMatchNum = -1;
    for (const Match& ma : matchesAsPlayer)
    {
      ObjState stat = ma.getState();
      int maNum = ma.getMatchNumber();

      // count all scheduled matches
      if (maNum != MatchNumNotAssigned) ++scheduledCount;

      if (stat == ObjState::MA_Running)
      {
        currentMatchId = ma.getId();
        continue;
      }

      if (stat == ObjState::MA_Finished)
      {
        ++finishCount;
        QDateTime fTime = ma.getFinishTime();
        if (fTime.isValid())
        {
          if ((!(lastFinishTime.isValid())) || (fTime > lastFinishTime))
          {
            lastFinishTime = fTime;
            lastPlayedMatchId = ma.getId();
          }
        } else {
          // invalid finish time indicates a walkover
          ++walkoverCount;
        }
        continue;
      }

      if ((maNum != MatchNumNotAssigned) && ((maNum < nextMatchNum) || (nextMatchNum < 0)))
      {
        nextMatchNum = maNum;
        nextMatchId = ma.getId();
      }
    }
  }

  //----------------------------------------------------------------------------

  void PlayerProfile::initMatchLists()
  {
    //
    // find all matches involving the participant as a PLAYER
    //

    // step 1: search via PlayerPairs
    QString where = "%1=%2 OR %3=%2";
    where = where.arg(Pairs_Player1Ref);
    where = where.arg(p.getId());
    where = where.arg(Pairs_Player2Ref);
    DbTab pairsTab{db, TabPairs, false};
    auto rows = pairsTab.getRowsByWhereClause(where.toUtf8().constData());
    std::vector<int> matchIdList;

    DbTab matchTab{db, TabMatch, false};
    MatchMngr mm{db};
    for (const auto& r : rows)
    {
      int ppId = r.id();

      // search for all matches involving this player pair
      where = "%1=%2 OR %3=%2";
      where = where.arg(MA_Pair1Ref);
      where = where.arg(ppId);
      where = where.arg(MA_Pair2Ref);
      auto matchRows = matchTab.getRowsByWhereClause(where.toUtf8().constData());
      for (const auto& matchRow : matchRows)
      {
        int maId = matchRow.id();
        if (!(Sloppy::isInVector<int>(matchIdList, maId))) matchIdList.push_back(maId);
      }
    }

    // step 2: search via ACTUAL_PLAYER
    where = "%1=%2 OR %3=%2 OR %4=%2 OR %5=%2";
    where = where.arg(MA_ActualPlayer1aRef);
    where = where.arg(p.getId());
    where = where.arg(MA_ActualPlayer1bRef);
    where = where.arg(MA_ActualPlayer2aRef);
    where = where.arg(MA_ActualPlayer2bRef);
    auto matchRows = matchTab.getRowsByWhereClause(where.toUtf8().constData());
    for (const auto& matchRow : matchRows)
    {
      int maId = matchRow.id();
      if (!(Sloppy::isInVector<int>(matchIdList, maId))) matchIdList.push_back(maId);
    }

    // copy the results to the matchesAsPlayer-list
    for (int maId : matchIdList)
    {
      auto ma = mm.getMatch(maId);
      matchesAsPlayer.push_back(*ma);
    }

    // sort by match number
    std::sort(matchesAsPlayer.begin(), matchesAsPlayer.end(), [](const Match& ma1, const Match& ma2)
    {
      return ma1.getMatchNumber() < ma2.getMatchNumber();
    });


    //
    // find all matches involving the participant as an UMPIRE
    //
    matchIdList.clear();
    matchRows = matchTab.getRowsByColumnValue(MA_RefereeRef, p.getId());
    for (const auto& matchRow : matchRows)
    {
      int maId = matchRow.id();

      auto ma = mm.getMatch(maId);
      matchesAsUmpire.push_back(*ma);
    }

    // sort by match number
    std::sort(matchesAsUmpire.begin(), matchesAsUmpire.end(), [](const Match& ma1, const Match& ma2)
    {
      return ma1.getMatchNumber() < ma2.getMatchNumber();
    });

  }

  //----------------------------------------------------------------------------

  std::optional<Match> PlayerProfile::returnMatchOrEmpty(int maId) const
  {
    if (maId < 1) return {};

    MatchMngr mm{db};
    return mm.getMatch(maId);
  }

  //----------------------------------------------------------------------------

}
