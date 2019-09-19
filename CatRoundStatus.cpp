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

#include <exception>

#include "CatRoundStatus.h"
#include "MatchMngr.h"

namespace QTournament
{

CatRoundStatus::CatRoundStatus(const TournamentDB& _db, const Category& _cat)
  :db(_db), cat(_cat)
{
}

//----------------------------------------------------------------------------

QList<int> CatRoundStatus::getCurrentlyRunningRoundNumbers() const
{
  QList<int> result;

  int lastFinishedRound = getFinishedRoundsCount();
  int minRoundRunning = -1;
  int maxRoundRunning = -1;

  // the initial round that could be in state RUNNING
  int roundToCheck = (lastFinishedRound < 0) ? 1 : lastFinishedRound+1;

  // the last round that can possibly in state RUNNING
  MatchMngr mm{db};
  int lastRoundToCheck = mm.getHighestUsedRoundNumberInCategory(cat);

  // loop through all applicable rounds and check their status
  while (roundToCheck <= lastRoundToCheck)
  {
    bool hasFinishedMatches = cat.hasMatchesInState(ObjState::MA_Finished, roundToCheck);
    bool hasRunningMatches = cat.hasMatchesInState(ObjState::MA_Running, roundToCheck);

    if (hasFinishedMatches || hasRunningMatches)
    {
      if (minRoundRunning < 0) minRoundRunning = roundToCheck;
      if (maxRoundRunning < roundToCheck) maxRoundRunning = roundToCheck;
      result.append(roundToCheck);
    }

    ++roundToCheck;
  }

  return result;
}

//----------------------------------------------------------------------------

int CatRoundStatus::getHighestGeneratedMatchRound() const
{
  MatchMngr mm{db};
  return mm.getHighestUsedRoundNumberInCategory(cat);
}

//----------------------------------------------------------------------------

int CatRoundStatus::getCurrentlyRunningRoundNumber() const
{
  QList<int> runningRounds = getCurrentlyRunningRoundNumbers();
  int n = runningRounds.count();

  if (n < 1) return NoCurrentlyRunningRounds;
  if (n == 1) return runningRounds.at(0);

  return MultipleRoundsRunning;  // can happen in group match rounds
}

//----------------------------------------------------------------------------

int CatRoundStatus::getFinishedRoundsCount() const
{
  MatchMngr mm{db};

  int roundNum = 1;
  int lastFinishedRound = NoRoundsFinishedYet;

  // go through rounds one by one
  while (true)
  {
    MatchGroupList matchGroupsInThisRound = mm.getMatchGroupsForCat(cat, roundNum);
    // finish searching for rounds when no more groups show up
    // in the search
    if (matchGroupsInThisRound.size() == 0) break;

    bool allGroupsFinished = true;
    for (MatchGroup mg : matchGroupsInThisRound)
    {
      if (mg.is_NOT_InState(ObjState::MG_Finished))
      {
        allGroupsFinished = false;
        break;
      }
    }

    if (!allGroupsFinished) break;

    // we only make it to this point if all match groups
    // in this round are finished.
    // so we may safely assume that this round is finished
    lastFinishedRound = roundNum;

    // check next round
    ++roundNum;
  }

  return lastFinishedRound;
}

//----------------------------------------------------------------------------

std::tuple<int, int, int> CatRoundStatus::getMatchCountForCurrentRound() const
{
  QList<int> runningRounds = getCurrentlyRunningRoundNumbers();

  if (runningRounds.count() == 0)
  {
    int tmp = NoCurrentlyRunningRounds;
    return std::tuple{tmp, tmp, tmp};
  }

  int unfinishedMatchCount = 0;
  int runningMatchCount = 0;
  int totalMatchCount = 0;

  MatchMngr mm{db};
  for (int curRound : runningRounds)
  {
    MatchGroupList matchGroupsInThisRound = mm.getMatchGroupsForCat(cat, curRound);
    for (MatchGroup mg : matchGroupsInThisRound)
    {
      for (Match ma : mg.getMatches())
      {
        if (ma.is_NOT_InState(ObjState::MA_Finished)) ++unfinishedMatchCount;
        if (ma.isInState(ObjState::MA_Running)) ++runningMatchCount;
        ++totalMatchCount;
      }
    }
  }

  // total, unfinished, running
  return std::tuple{totalMatchCount, unfinishedMatchCount, runningMatchCount};
}

//----------------------------------------------------------------------------

int CatRoundStatus::getTotalRoundsCount() const
{
  auto specializedCat = cat.convertToSpecializedObject();
  return specializedCat->calcTotalRoundsCount();
}

//----------------------------------------------------------------------------


//----------------------------------------------------------------------------

}
