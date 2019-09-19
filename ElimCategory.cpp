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

#include <QDebug>

#include <Sloppy/Utils.h>

#include <SqliteOverlay/Transaction.h>

#include "ElimCategory.h"
#include "KO_Config.h"
#include "CatRoundStatus.h"
#include "RankingEntry.h"
#include "RankingMngr.h"
#include "assert.h"
#include "HelperFunc.h"
#include "MatchMngr.h"
#include "CatMngr.h"
#include "RankingMngr.h"
#include "reports/BracketVisData.h"

using namespace SqliteOverlay;

namespace QTournament
{

  EliminationCategory::EliminationCategory(const TournamentDB& _db, int rowId, int eliminationMode)
  : Category(_db, rowId)
  {
    if ((eliminationMode != BracketGenerator::BracketSingleElim) &&
        (eliminationMode != BracketGenerator::BracketDoubleElim) &&
        (eliminationMode != BracketGenerator::BracketRanking1))
    {
      throw std::invalid_argument("Invalid elimination mode in ctor of EliminationCategory!");
    }

    elimMode = eliminationMode;
  }

//----------------------------------------------------------------------------

  EliminationCategory::EliminationCategory(const TournamentDB& _db, const TabRow& _row, int eliminationMode)
  : Category(_db, _row)
  {
    if ((eliminationMode != BracketGenerator::BracketSingleElim) &&
        (eliminationMode != BracketGenerator::BracketDoubleElim) &&
        (eliminationMode != BracketGenerator::BracketRanking1))
    {
      throw std::invalid_argument("Invalid elimination mode in ctor of EliminationCategory!");
    }

    elimMode = eliminationMode;
  }

//----------------------------------------------------------------------------

  Error EliminationCategory::canFreezeConfig()
  {
    if (is_NOT_InState(ObjState::CAT_Config))
    {
      return Error::ConfigAlreadyFrozen;
    }
    
    // make sure there no unpaired players in singles or doubles
    if ((getMatchType() != MatchType::Singles) && (hasUnpairedPlayers()))
    {
      return Error::UnpairedPlayers;
    }

    // we should have at least two players / pairs
    int numPairs = getAllPlayersInCategory().size();
    if (getMatchType() != MatchType::Singles)
    {
      numPairs = numPairs / 2;    // numPairs before division must be even, because we had no unpaired players (see check above)
    }
    if (numPairs < 2)
    {
      return Error::InvalidPlayerCount;
    }

    // for the bracket mode "ranking1" we may not have more
    // than 32 players
    if ((elimMode == BracketGenerator::BracketRanking1) && (numPairs > 32))
    {
      return Error::InvalidPlayerCount;
    }

    return Error::OK;
  }

//----------------------------------------------------------------------------

  bool EliminationCategory::needsInitialRanking()
  {
    return true;
  }

//----------------------------------------------------------------------------

  bool EliminationCategory::needsGroupInitialization()
  {
    return false;
  }

//----------------------------------------------------------------------------

  Error EliminationCategory::prepareFirstRound()
  {
    if (is_NOT_InState(ObjState::CAT_Idle)) return Error::WrongState;

    MatchMngr mm{db};

    // make sure we have not been called before; to this end, just
    // check that there have no matches been created for us so far
    auto allGrp = mm.getMatchGroupsForCat(*this);
    // do not return an error here, because obviously we have been
    // called successfully before and we only want to avoid
    // double initialization
    if (allGrp.size() != 0) return Error::OK;

    // alright, this is a virgin category. Generate bracket matches
    // for each group
    CatMngr cm{db};
    PlayerPairList seeding = cm.getSeeding(*this);
    return generateBracketMatches(elimMode, seeding, 1);
  }

//----------------------------------------------------------------------------

  int EliminationCategory::calcTotalRoundsCount() const
  {
    ObjState stat = getState();
    if ((stat == ObjState::CAT_Config) || (stat == ObjState::CAT_Frozen))
    {
      return -1;   // category not yet fully configured; can't calc rounds
    }

    BracketGenerator bg{elimMode};
    int numPairs = getDatabasePlayerPairCount();
    return bg.getNumRounds(numPairs);
  }

//----------------------------------------------------------------------------

  // this returns a function that should return true if "a" goes before "b" when sorting. Read:
  // return a function that returns true true if the score of "a" is better than "b"
  std::function<bool (RankingEntry& a, RankingEntry& b)> EliminationCategory::getLessThanFunction()
  {
    return [](RankingEntry& a, RankingEntry& b) {
      return false;   // there is no definite ranking in elimination rounds, so simply return a dummy value
    };
  }

//----------------------------------------------------------------------------

  Error EliminationCategory::onRoundCompleted(int round)
  {
    // create ranking entries for everyone who played
    // and for everyone who achieved a final rank in a
    // previous round
    Error err;
    RankingMngr rm{db};
    PlayerPairList ppList;
    if (round == 1)
    {
      ppList = getPlayerPairs();
    } else {
      ppList = this->getRemainingPlayersAfterRound(round - 1, &err);
      if (err != Error::OK) return err;
    }
    auto rll = rm.getSortedRanking(*this, round-1);
    for (auto rl : rll)
    {
      for (RankingEntry re : rl)
      {
        if (re.getRank() != RankingEntry::NoRankAssigned)
        {
          auto pp = re.getPlayerPair();
          assert(pp);
          bool hasPair = (std::find(ppList.begin(), ppList.end(), *pp) != ppList.end());
          if (!hasPair)
          {
            ppList.push_back(*pp);
          }
        }
      }
    }

    // create unsorted entries for everyone who played in this round
    // or who achieved a final rank in a previous round
    rm.createUnsortedRankingEntriesForLastRound(*this, &err, ppList);
    if (err != Error::OK) return err;

    // set the rank for all players that ended up at a final rank
    // in this or any prior round
    err = rewriteFinalRankForMultipleRounds(round, round);

    return err;
  }

//----------------------------------------------------------------------------

  PlayerPairList EliminationCategory::getRemainingPlayersAfterRound(int round, Error* err) const
  {
    int lastRoundInThisCat = calcTotalRoundsCount();
    if (round == lastRoundInThisCat)
    {
      if (err != nullptr) *err = Error::OK;
      return PlayerPairList();  // no remaining players after last round
    }

    // we can only determine remaining players after completed rounds
    CatRoundStatus crs = getRoundStatus();
    if (round > crs.getFinishedRoundsCount())
    {
      if (err != nullptr) *err = Error::InvalidRound;
      return PlayerPairList();
    }

    // get the list for the previous round, if any
    PlayerPairList result;
    if (round > 0)
    {
      Error e;
      result = this->getRemainingPlayersAfterRound(round-1, &e);
      if (e != Error::OK)
      {
        if (err != nullptr) *err = Error::InvalidRound;
        return PlayerPairList();
      }
    } else {
      // round 0 (before first round)
      if (err != nullptr) *err = Error::OK;
      return getPlayerPairs();
    }

    // now that we have the survivors of the previous round (or
    // the initial list of all players if this is round one) we
    // sort out all players are not further used in future matches
    //
    // for this, we walk through all matches in this round and remove
    // those players that have no future matches.
    //
    // "no future match" can mean player "eliminated" or "ranked"
    MatchMngr mm{db};
    for (MatchGroup mg : mm.getMatchGroupsForCat(*this, round))
    {
      for (Match ma : mg.getMatches())
      {
        auto loser = ma.getLoser();
        assert(loser);
        int loserPairId = loser->getPairId();
        assert(loserPairId > 0);

        auto winner = ma.getWinner();
        assert(winner);
        int winnerPairId = winner->getPairId();
        assert(winnerPairId > 0);

        bool winnerOut = false;
        bool loserOut = false;

        // check 1: is there a final rank for the winner?
        if (ma.getWinnerRank() > 0)
        {
          Sloppy::eraseAllOccurencesFromVector<PlayerPair>(result, *winner);
          winnerOut = true;
        }

        // check 2: is there a final rank for the loser?
        if (ma.getLoserRank() > 0)
        {
          Sloppy::eraseAllOccurencesFromVector<PlayerPair>(result, *loser);
          loserOut = true;
        }

        //
        // Intermezzo: a helper function for searching
        // for future matches of a pair ID
        //
        DbTab matchTab{db, TabMatch, false};
        auto hasFutureMatch = [&](const PlayerPair& pp, bool asWinner) {
          // step 1: search by pair
          for (int r=round+1; r <= lastRoundInThisCat; ++r)
          {
            auto next = mm.getMatchForPlayerPairAndRound(pp, r);
            if (next)
            {
              return true;
            }
          }

          // step 2: search for "is winner of" or "is loser of"
          // this match
          Sloppy::estring where = "%1 = %3 OR %2 = %3";
          where.arg(MA_Pair1SymbolicVal);
          where.arg(MA_Pair2SymbolicVal);
          int symbMatchId = asWinner ? ma.getId() : -(ma.getId());
          where.arg(symbMatchId);
          if (matchTab.getMatchCountForWhereClause(where) > 0)
          {
            return true;
          }

          return false;
        };
        //   --------- Intermezzo end -----------------


        // check 3: if the winner is still in: is there
        // a future game in this category for the winner?
        if (!winnerOut)
        {
          if (!(hasFutureMatch(*winner, true)))
          {
            Sloppy::eraseAllOccurencesFromVector<PlayerPair>(result, *winner);
          }
        }

        // check 4: if the loser is still in: is there
        // a future game in this category for the loser?
        if (!loserOut)
        {
          if (!(hasFutureMatch(*loser, false)))
          {
            Sloppy::eraseAllOccurencesFromVector<PlayerPair>(result, *loser);
          }
        }
      }
    }

    // everyone who has not yet been kicked from the
    // list survives this round
    Sloppy::assignIfNotNull<Error>(err, Error::OK);
    return result;
  }

  //----------------------------------------------------------------------------

  ModMatchResult EliminationCategory::canModifyMatchResult(const Match& ma) const
  {
    // the match has to be in FINISHED state
    if (ma.is_NOT_InState(ObjState::MA_Finished)) return ModMatchResult::NotPossible;

    // if this match does not belong to us, we're not responsible
    if (ma.getCategory().getId() != getId()) return ModMatchResult::NotPossible;

    // if the winner's and the loser's match have both not yet been started,
    // we can still change the winner/loser. Otherwise we can only apply
    // cosmetic changes to the score
    auto winnerMatch = getFollowUpMatch(ma, false);
    auto loserMatch = getFollowUpMatch(ma, true);
    bool canModWinnerLoser = true;
    if (winnerMatch)
    {
      ObjState stat = winnerMatch->getState();
      if ((stat == ObjState::MA_Running) || (stat == ObjState::MA_Finished))
      {
        canModWinnerLoser = false;
      }
    }
    if (loserMatch)
    {
      ObjState stat = loserMatch->getState();
      if ((stat == ObjState::MA_Running) || (stat == ObjState::MA_Finished))
      {
        canModWinnerLoser = false;
      }
    }

    return canModWinnerLoser ? ModMatchResult::WinnerLoser : ModMatchResult::ScoreOnly;
  }

  //----------------------------------------------------------------------------

  ModMatchResult EliminationCategory::modifyMatchResult(const Match& ma, const MatchScore& newScore) const
  {
    ModMatchResult mmr = canModifyMatchResult(ma);
    if ((mmr != ModMatchResult::ScoreOnly) && (mmr != ModMatchResult::WinnerLoser)) return mmr;

    // does the new score modify the winner/loser?
    MatchScore oldScore = *(ma.getScore());
    bool isWinnerMod = (oldScore.getWinner() != newScore.getWinner());

    // if the new score modifies the winner / loser
    // and this was not permitted, we return with "ScoreOnly" to indicate an error
    if ((mmr == ModMatchResult::ScoreOnly) && isWinnerMod)
    {
      return ModMatchResult::ScoreOnly;
    }

    // start a new database transaction to ensure
    // consistent modifications
    try
    {
      auto trans = db.get().startTransaction();

      // swap winner / loser in the follow-up matches
      MatchMngr mm{db};
      if (isWinnerMod)
      {
        PlayerPair oldWinner = *(ma.getWinner());
        PlayerPair oldLoser = *(ma.getLoser());

        auto winnerMatch = getFollowUpMatch(ma, false);
        auto loserMatch = getFollowUpMatch(ma, true);

        if (winnerMatch)
        {
          Error e = mm.swapPlayer(*winnerMatch, oldWinner, oldLoser);
          if (e != Error::OK) return ModMatchResult::NotPossible;   // triggers implicit rollback
        }
        if (loserMatch)
        {
          Error e = mm.swapPlayer(*loserMatch, oldLoser, oldWinner);
          if (e != Error::OK) return ModMatchResult::NotPossible;  // triggers implicit rollback
        }

        // delete explicit references to the affected pair in the
        // bracket visualization
        auto bvd = BracketVisData::getExisting(ma.getCategory());
        if (bvd)
        {
          bvd->clearExplicitPlayerPairReferences(oldWinner);
          bvd->clearExplicitPlayerPairReferences(oldLoser);
        }
      }

      // update the match score
      Error e = mm.updateMatchScore(ma, newScore, (mmr == ModMatchResult::WinnerLoser));
      if (e != Error::OK)
      {
        return ModMatchResult::NotPossible;  // triggers implicit rollback
      }

      // update the ranking entries but skip the assignment of ranks
      RankingMngr rm{db};
      e = rm.updateRankingsAfterMatchResultChange(ma, oldScore, true);
      if (e != Error::OK) return ModMatchResult::NotPossible;  // triggers implicit rollback

      // the previous call did not properly update the assigned
      // ranks, because ranking in bracket matches works different
      // than in other match system.
      // thus, we call a special function that modifies
      // the ranks directly.
      //
      // we only need to do this if we modified a match of a completed
      // round. otherwise there aren't any RankingEntries to modify at all
      CatRoundStatus crs = getRoundStatus();
      if (ma.getMatchGroup().getRound() <= crs.getFinishedRoundsCount())
      {
        e = rewriteFinalRankForMultipleRounds(ma.getMatchGroup().getRound());
        if (e != Error::OK) return ModMatchResult::NotPossible;  // triggers implicit rollback
      }

      trans.commit();

      return ModMatchResult::ModDone;
    }
    catch(...)
    {
      return ModMatchResult::NotPossible;
    }
  }

  //----------------------------------------------------------------------------

  std::optional<Match> EliminationCategory::getFollowUpMatch(const Match& ma, bool searchLoserNotWinner) const
  {
    if (ma.getCategory().getId() != getId()) return {};

    //
    // There are two solutions:
    // (1) the match has already been finished. In this case we must search
    //     for a match in a subsequent round that includes the winner/loser
    //
    // (2) the has not been finished and so we have to search via
    //     symbolic match references.
    //

    //
    // Case 1: the match has been finished
    //
    ObjState stat = ma.getState();
    if (stat == ObjState::MA_Finished)
    {
      PlayerPair pp = searchLoserNotWinner ? *(ma.getLoser()) : *(ma.getWinner());
      int round = ma.getMatchGroup().getRound() + 1;
      int maxRound = calcTotalRoundsCount();

      // find all groups for rounds later than "round"
      MatchMngr mm{db};
      while (round <= maxRound)
      {
        auto result = mm.getMatchForPlayerPairAndRound(pp, round);
        if (result) return result;
        ++round;
      }
      return {};  // no match found
    }

    //
    // Case 2: the match has not yet been finished
    //
    int maId = searchLoserNotWinner ? -ma.getId() : ma.getId();
    DbTab mTab{db, TabMatch, false};
    auto resultRow = mTab.getSingleRowByColumnValue2(MA_Pair1SymbolicVal, maId);
    if (!resultRow)
    {
      resultRow = mTab.getSingleRowByColumnValue2(MA_Pair2SymbolicVal, maId);
    }

    if (!resultRow) return {};
    MatchMngr mm{db};
    return mm.getMatch(resultRow->id());
  }

  //----------------------------------------------------------------------------

  Error EliminationCategory::rewriteFinalRankForMultipleRounds(int minRound, int maxRound) const
  {
    // some boundary checks
    if (minRound < 1) return Error::InvalidRound;
    CatRoundStatus crs = getRoundStatus();
    int lastCompletedRound = crs.getFinishedRoundsCount();
    if (lastCompletedRound < 1) return Error::InvalidRound;
    if (minRound > lastCompletedRound) return Error::InvalidRound;
    if (maxRound < 1) maxRound = lastCompletedRound;
    if (maxRound < minRound) return Error::InvalidRound;
    if (maxRound > lastCompletedRound) return Error::InvalidRound;

    // start a pretty inefficient algorithm that goes through
    // all rounds from "min" to "max" and loop over all
    // round from "1" to "current" in every itegration...
    MatchMngr mm{db};
    RankingMngr rm{db};
    for (int curRound = minRound; curRound <= maxRound; ++curRound)
    {
      std::vector<int> pairsWithRank;

      for (int r=1; r <= curRound; ++r)
      {
        for (const MatchGroup& mg : mm.getMatchGroupsForCat(*this, r))
        {
          for (const Match& ma : mg.getMatches())
          {
            int winnerRank = ma.getWinnerRank();
            if (winnerRank > 0)
            {
              // we never go beyong the last completed round,
              // so we should always have a winner and an
              // associated (unsorted) ranking entry
              auto w = ma.getWinner();
              assert(w);
              auto re = rm.getRankingEntry(*w, curRound);
              assert(re);
              rm.forceRank(*re, winnerRank);
              pairsWithRank.push_back(w->getPairId());
            }

            int loserRank = ma.getLoserRank();
            if (loserRank > 0)
            {
              // we never go beyong the last completed round,
              // so we should always have a loser and an
              // associated (unsorted) ranking entry
              auto l = ma.getLoser();
              assert(l);
              auto re = rm.getRankingEntry(*l, curRound);
              assert(re);
              rm.forceRank(*re, loserRank);
              pairsWithRank.push_back(l->getPairId());
            }
          }
        }
      }

      // clear the rank of all "unranked" pairs, just be sure
      // (otherwise, stale ranks might survive a
      // change of a match result)
      for (const PlayerPair& pp : getPlayerPairs())
      {
        // skip all PlayerPairs with an already assigned rank
        if (Sloppy::isInVector<int>(pairsWithRank, pp.getPairId())) continue;

        // set the rank to "Not assigned"
        auto re = rm.getRankingEntry(pp, curRound);
        if (re)
        {
          rm.clearRank(*re);
        }
      }
    }

    return Error::OK;
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


}
