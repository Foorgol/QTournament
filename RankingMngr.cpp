/* 
 * File:   TeamMngr.cpp
 * Author: nyoknvk1
 * 
 * Created on 18. Februar 2014, 14:04
 */

#include <stdexcept>
#include <algorithm>

#include "RankingMngr.h"
#include "Tournament.h"
#include "CatRoundStatus.h"
#include "RankingEntry.h"
#include "Match.h"
#include "Score.h"

using namespace dbOverlay;

namespace QTournament
{

  RankingMngr::RankingMngr(TournamentDB* _db)
  : GenericObjectManager(_db), rankTab((*db)[TAB_RANKING])
  {
  }

//----------------------------------------------------------------------------

  RankingEntryList RankingMngr::createUnsortedRankingEntriesForLastRound(const Category &cat, ERR *err, PlayerPairList _ppList, bool reset)
  {
    // determine the round we should create the entries for
    CatRoundStatus crs = cat.getRoundStatus();
    int lastRound = crs.getFinishedRoundsCount();
    if (lastRound < 1)
    {
      if (err != nullptr) *err = ROUND_NOT_FINISHED;
      return RankingEntryList();
    }

    // establish the list of player pairs for that the ranking shall
    // be created; it's either ALL pairs or a caller-provided list
    PlayerPairList ppList;
    if (_ppList.isEmpty())
    {
      ppList = cat.getPlayerPairs();
    } else {
      ppList = _ppList;
    }

    // make sure that all matches for these player pairs have
    // valid results. We need to check this in a separate loop
    // to avoid that the ranking entries have already been
    // halfway written to the database when we encore an invalid
    // match
    for (PlayerPair pp : ppList)
    {
      unique_ptr<Match> ma = Tournament::getMatchMngr()->getMatchForPlayerPairAndRound(pp, lastRound);
      if (ma != nullptr)
      {
        ERR e;
        unique_ptr<MatchScore> score = ma->getScore(&e);
        if (e != OK)
        {
          if (err != nullptr) *err = e;
          return RankingEntryList();
        }
        if (score == nullptr)
        {
          if (err != nullptr) *err = NO_MATCH_RESULT_SET;
          return RankingEntryList();
        }
      }
    }

    //
    // Okay, we can be pretty sure that the rest of this method
    // succeeds and that we leave the database in a consistent state
    //


    // iterate over the player pair list and create entries
    // based on match result and, possibly, previous ranking entries
    RankingEntryList result;
    for (PlayerPair pp : ppList)
    {
      // prepare the values for the new entry
      int wonMatches = 0;
      int drawMatches = 0;
      int lostMatches = 0;
      int wonGames = 0;
      int lostGames = 0;
      int wonPoints = 0;
      int lostPoints = 0;

      // get match results, if the player played in this round
      // (maybe the player had a bye; in this case we skip this section)
      unique_ptr<Match> ma = Tournament::getMatchMngr()->getMatchForPlayerPairAndRound(pp, lastRound);
      if (ma != nullptr)
      {
        // determine whether our pp is player 1 or player 2
        int pp1Id = ma->getPlayerPair1().getPairId();
        int playerNum = (pp1Id == pp.getPairId()) ? 1 : 2;

        // create column values for match data
        ERR e;
        unique_ptr<MatchScore> score = ma->getScore(&e);
        if (score == nullptr) qDebug() << "!!! NULL !!!";
        if (e != OK) qDebug() << e;
        wonMatches = (score->getWinner() == playerNum) ? 1 : 0;
        lostMatches = (score->getLoser() == playerNum) ? 1 : 0;
        drawMatches = (score->getWinner() == 0) ? 1 : 0;

        // create column values for game data
        tuple<int, int> gameSum = score->getGameSum();
        int gamesTotal = get<0>(gameSum) + get<1>(gameSum);
        wonGames = (playerNum == 1) ? get<0>(gameSum) : get<1>(gameSum);
        lostGames = gamesTotal - wonGames;

        // create column values for point data
        tuple<int, int> scoreSum = score->getScoreSum();
        wonPoints = (playerNum == 1) ? get<0>(scoreSum) : get<1>(scoreSum);
        lostPoints = score->getPointsSum() - wonPoints;
      }

      // add values from previous round, if required
      unique_ptr<RankingEntry> prevEntry = nullptr;
      if (!reset)
      {
        // the next call may return nullptr, but this is fine.
        // it just means that there is no old entry to build upon
        // and thus we start with the scoring at zero
        prevEntry = getRankingEntry(pp, lastRound-1);
      }

      if (prevEntry != nullptr)
      {
        tuple<int, int, int, int> maStat = prevEntry->getMatchStats();
        wonMatches += get<0>(maStat);
        drawMatches += get<1>(maStat);
        lostMatches += get<2>(maStat);

        tuple<int, int, int> gameStat = prevEntry->getGameStats();
        wonGames += get<0>(gameStat);
        lostGames += get<1>(gameStat);

        tuple<int, int> pointStat = prevEntry->getPointStats();
        wonPoints += get<0>(pointStat);
        lostPoints += get<1>(pointStat);
      }

      // prep the complete data set for the entry,
      // but leave the "rank" column empty
      QVariantList qvl;
      qvl << RA_MATCHES_WON << wonMatches;
      qvl << RA_MATCHES_DRAW << drawMatches;
      qvl << RA_MATCHES_LOST << lostMatches;
      qvl << RA_GAMES_WON << wonGames;
      qvl << RA_GAMES_LOST << lostGames;
      qvl << RA_POINTS_WON << wonPoints;
      qvl << RA_POINTS_LOST << lostPoints;
      qvl << RA_PAIR_REF << pp.getPairId();
      qvl << RA_ROUND << lastRound;
      qvl << RA_CAT_REF << cat.getId();  // eases searching, but is redundant information
      qvl << RA_GRP_NUM << pp.getPairsGroupNum(db); // eases searching, but is redundant information

      // create the new entry and add an instance
      // of the entry to the result list
      int newId = rankTab.insertRow(qvl);
      result.append(RankingEntry(db, newId));
    }

    if (err != nullptr) *err = OK;
    return result;
  }

//----------------------------------------------------------------------------

  unique_ptr<RankingEntry> RankingMngr::getRankingEntry(const PlayerPair &pp, int round) const
  {
    QVariantList qvl;
    qvl << RA_CAT_REF << pp.getCategory(db)->getId();
    qvl << RA_PAIR_REF << pp.getPairId();
    qvl << RA_ROUND << round;

    return getSingleObjectByColumnValue<RankingEntry>(rankTab, qvl);
  }

//----------------------------------------------------------------------------

  RankingEntryListList RankingMngr::getSortedRanking(const Category &cat, int round) const
  {
    RankingEntryListList result;

    // get separate lists for every match group.
    //
    // In non-round-robin matches, this does no harm because
    // there is only one (artificial) match group in those cases
    for (MatchGroup mg : Tournament::getMatchMngr()->getMatchGroupsForCat(cat, round))
    {
      QString where = RA_CAT_REF + " = " + QString::number(cat.getId());
      where += " AND " + RA_ROUND + " = " + QString::number(round);
      where += " AND " + RA_GRP_NUM + " = " + QString::number(mg.getGroupNumber());
      where += " ORDER BY " + RA_RANK + " ASC";

      result.append(getObjectsByWhereClause<RankingEntry>(rankTab, where));
    }

    return result;
  }

//----------------------------------------------------------------------------

  RankingEntryListList RankingMngr::sortRankingEntriesForLastRound(const Category& cat, ERR* err) const
  {
    // determine the round we should create the entries for
    CatRoundStatus crs = cat.getRoundStatus();
    int lastRound = crs.getFinishedRoundsCount();
    if (lastRound < 1)
    {
      if (err != nullptr) *err = ROUND_NOT_FINISHED;
      return RankingEntryListList();
    }

    // make sure we have (unsorted) ranking entries
    QVariantList qvl;
    qvl << RA_CAT_REF << cat.getId();
    qvl << RA_ROUND << lastRound;
    if (rankTab.getMatchCountForColumnValue(qvl) < 1)
    {
      if (err != nullptr) *err = MISSING_RANKING_ENTRIES;
      return RankingEntryListList();
    }

    // get the category-specific comparison function
    auto specializedCat = cat.convertToSpecializedObject();
    auto lessThanFunc = specializedCat->getLessThanFunction();

    // prepare the result object
    RankingEntryListList result;

    // apply separate sorting for every match group.
    //
    // In non-round-robin matches, this does no harm because
    // there is only one (artificial) match group in those cases
    qvl << RA_GRP_NUM << 4242;  // dummy number, just to fill the field
    for (MatchGroup mg : Tournament::getMatchMngr()->getMatchGroupsForCat(cat, lastRound))
    {
      // remove last group number
      // and append the current one
      qvl.removeLast();
      qvl << mg.getGroupNumber();

      // get the ranking entries
      RankingEntryList rankList = getObjectsByColumnValue<RankingEntry>(rankTab, qvl);

      // call the standard sorting algorithm
      std::sort(rankList.begin(), rankList.end(), lessThanFunc);

      // write the sort results back to the database
      int rank = 1;
      for (RankingEntry re : rankList)
      {
        re.row.update(RA_RANK, rank);
        ++rank;
      }

      // add the sorted group list to the result
      result.append(rankList);
    }

    if (err != nullptr) *err = OK;
    return result;
  }

//----------------------------------------------------------------------------

  unique_ptr<RankingEntry> RankingMngr::getRankingEntry(const Category& cat, int round, int grpNum, int rank) const
  {
    QVariantList qvl;
    qvl << RA_CAT_REF << cat.getId();
    qvl << RA_ROUND << round;
    qvl << RA_GRP_NUM << grpNum;
    qvl << RA_RANK << rank;

    return getSingleObjectByColumnValue<RankingEntry>(rankTab, qvl);
  }

//----------------------------------------------------------------------------
//----------------------------------------------------------------------------
//----------------------------------------------------------------------------
//----------------------------------------------------------------------------
//----------------------------------------------------------------------------


}
