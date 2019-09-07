
#include "Match.h"

#include "BracketMatchData.h"
#include "SvgBracket.h"

using namespace std;

namespace QTournament::SvgBracket
{
  BracketMatchData::BracketMatchData(const BracketMatchNumber& n, const Rank& p1InitialRank, const Rank& p2InitialRank, const std::variant<OutgoingBracketLink, Rank>& dstWinner, const std::variant<OutgoingBracketLink, Rank>& dstLoser)
    :brMaNum{n}, _round{1}, winnerAction{dstWinner}, loserAction{dstLoser}, src1{p1InitialRank}, src2{p2InitialRank},
      p1State{BranchState::Alive}, p2State{BranchState::Alive}
  {

  }

  //----------------------------------------------------------------------------

  BracketMatchData::BracketMatchData(const BracketMatchNumber& n, const Round& r, const IncomingBracketLink& inLink1, const IncomingBracketLink& inLink2, const std::variant<OutgoingBracketLink, Rank>& dstWinner, const std::variant<OutgoingBracketLink, Rank>& dstLoser)
    :brMaNum{n}, _round{r}, winnerAction{dstWinner}, loserAction{dstLoser}, src1{inLink1}, src2{inLink2},
      p1State{BranchState::Alive}, p2State{BranchState::Alive}
  {

  }

  //----------------------------------------------------------------------------

  std::optional<Rank> BracketMatchData::initialRank(int pos) const
  {
    if (_round.get() != 1) return std::optional<Rank>{};

    return (pos == 1) ? get<Rank>(src1) : get<Rank>(src2);
  }

  //----------------------------------------------------------------------------

  void BracketMatchData::assignPlayerPair(const PlayerPairRefId& ppId, int pos)
  {
    if (pos == 1)
    {
      p1Pair = ppId;
      p1State = BranchState::Assigned;
    } else {
      p2Pair = ppId;
      p2State = BranchState::Assigned;
    }
  }

  //----------------------------------------------------------------------------

  void BracketMatchData::setPairUnused(int pos)
  {
    if (pos == 1)
    {
      p1Pair = PlayerPairRefId{-1};;
      p1State = BranchState::Dead;
    } else {
      p2Pair = PlayerPairRefId{-1};;
      p2State = BranchState::Dead;
    }
    cout << "M " << brMaNum.get() << "." << pos << " = DEAD" << endl;
  }

  //----------------------------------------------------------------------------

  void BracketMatchDataList::applySeeding(const PlayerPairList& seed)
  {
    // step 1:
    // assign player pairs as far as possible
    std::for_each(begin(), end(), [&](BracketMatchData& bmd)
    {
      if (bmd.round().get() == 1)
      {
        for (int pos=1; pos < 3; ++pos)
        {
          Rank iniRank = bmd.initialRank(pos).value();
          if (iniRank.get() <= seed.size())
          {
            const auto& pp = seed.at(iniRank.get() - 1);
            const PlayerPairRefId ppId{pp.getPairId()};
            bmd.assignPlayerPair(ppId, pos);
            cout << "Seed assignment: M " << bmd.matchNum().get() << "." << pos << " = PlayerPair " << ppId.get() << endl;
          } else {
            bmd.setPairUnused(pos);
          }
        }
      }
    });

    // step 2:
    // apply all "fast forwards" in all rounds
    //
    // matches are sorted by number and thus implicitly by rounds ==> it is sufficient to iterate
    // over all matches
    std::for_each(begin(), end(), [&](BracketMatchData& bmd) { propagateWinnerLoser(bmd); });
  }

  //----------------------------------------------------------------------------

  void BracketMatchDataList::applyMatches(const std::vector<Match>& maList)
  {
    // create a copy of the match list and sort it
    // by bracket match number (which includes an implicit sorting by rounds)
    std::vector<Match> sortedMatches{maList};
    std::sort(std::begin(sortedMatches), std::end(sortedMatches), [](const Match& ma1, const Match& ma2)
    {
      return (ma1.bracketMatchNum() < ma2.bracketMatchNum());
    });

    // assign player pairs as far as possible
    for (const auto& ma : sortedMatches)
    {
      if (!ma.hasPlayerPair1() && !ma.hasPlayerPair2()) continue;

      auto brNum = ma.bracketMatchNum();
      BracketMatchData& bmd = findByMatchNumber(*brNum);

      if (ma.hasPlayerPair1())
      {
        const PlayerPairRefId ppId{ma.getPlayerPair1().getPairId()};
        bmd.assignPlayerPair(ppId, 1);
      } else {
        bmd.setPairUnused(1);
      }
      if (ma.hasPlayerPair2())
      {
        const PlayerPairRefId ppId{ma.getPlayerPair2().getPairId()};
        bmd.assignPlayerPair(ppId, 2);
      } else {
        bmd.setPairUnused(2);
      }

      // apply fast-forward logic to this match
      propagateWinnerLoser(bmd);
    }
  }

  //----------------------------------------------------------------------------

  void BracketMatchDataList::fastForward(BracketMatchData& ma, int pos)
  {
    PlayerPairRefId pairToForward = (pos == 1) ? ma.assignedPair1() : ma.assignedPair2();
    assert(pairToForward.get() > 0);

    // assign the player pair 1 of this match to the linked
    // player pair of the winner match
    if (ma.hasWinnerMatch())
    {
      const auto& winnerMatchInfo = ma.nextWinnerMatch();
      auto& winnerMatch = at(winnerMatchInfo.dstMatch.get() - 1);  // only works because the list is SORTED by match number
      winnerMatch.assignPlayerPair(pairToForward, winnerMatchInfo.pos);

      cout << "FastForward " << ma.matchNum().get() << "." << pos << " --> " << winnerMatchInfo.dstMatch.get() << "." << winnerMatchInfo.pos << endl;
    }

    // declare the loser branch dead
    if (ma.hasLoserMatch())
    {
      declareLoserBranchDead(ma);
    }
  }

  //----------------------------------------------------------------------------

  void BracketMatchDataList::declareLoserBranchDead(const BracketMatchData& ma)
  {
    const auto& loserMatchInfo = ma.nextLoserMatch();
    auto& loserMatch = at(loserMatchInfo.dstMatch.get() - 1);
    loserMatch.setPairUnused(loserMatchInfo.pos);
  }

  //----------------------------------------------------------------------------

  /*
  void BracketMatchDataList::propagateBackwardsAlongWinnerPath(const BracketMatchData& ma, int pos)
  {
    if (ma.round().get() == 1) return; // nothing to do for us

    const auto& inLink = (pos == 1) ? ma.inLink1() : ma.inLink2();
    if (inLink.role == PairRole::AsLoser)
    {
      // nothing to do for us, because if we came here as a loser
      // the previous match has been played as "real" match and thus
      // the previous match has fully assigned players
      return;
    }

    const BracketMatchData& prevMatch = findByMatchNumber(inLink.srcMatch);

  }
  */

  //----------------------------------------------------------------------------

  BracketMatchDataList::reference BracketMatchDataList::findByMatchNumber(const BracketMatchNumber& brNum)
  {
    auto it = find_if(begin(), end(), [&brNum](const BracketMatchData& bmd)
    {
      return (bmd.matchNum() == brNum);
    });

    return *it;
  }

  //----------------------------------------------------------------------------

  BracketMatchDataList::const_reference BracketMatchDataList::findByMatchNumber(const BracketMatchNumber& brNum) const
  {
    auto it = find_if(begin(), end(), [&brNum](const BracketMatchData& bmd)
    {
      return (bmd.matchNum() == brNum);
    });

    return *it;
  }

  //----------------------------------------------------------------------------

  void BracketMatchDataList::propagateWinnerLoser(BracketMatchData& ma)
  {
    using bs = BracketMatchData::BranchState;

    const auto& p1State = ma.pair1State();
    const auto& p2State = ma.pair2State();

    // Case 1: both branches have assigned players
    // ==> nothing to do for us
    if ((p1State == bs::Assigned) && (p2State == bs::Assigned)) return;

    // Case 2: both branches are "alive"
    // ==> nothing to do for us, the players depend on
    // other matches
    if ((p1State == bs::Alive) && (p2State == bs::Alive)) return;

    // Case 3: one branch assigned, one branch dead
    // ==> forward the assigned player to the winner match
    if ((p1State == bs::Assigned) && (p2State == bs::Dead))
    {
      fastForward(ma, 1);
    }
    if ((p1State == bs::Dead) && (p2State == bs::Assigned))
    {
      fastForward(ma, 2);
    }

    // Case 4: one branch alive, one branch dead
    // ==> declare the loser branch dead because the
    // one alive player will be "passed through" as winner
    if (((p1State == bs::Alive) && (p2State == bs::Dead)) || ((p1State == bs::Dead) && (p2State == bs::Alive)))
    {
      declareLoserBranchDead(ma);
    }

    // Case 5: both branches dead
    // ==> declare the winner and loser branches dead as well
    if ((p1State == bs::Dead) && (p2State == bs::Dead))
    {
      if (ma.hasWinnerMatch())
      {
        const auto& winnerMatchInfo = ma.nextWinnerMatch();
        auto& winnerMatch = at(winnerMatchInfo.dstMatch.get() - 1);  // only works because the list is SORTED by match number
        winnerMatch.setPairUnused(winnerMatchInfo.pos);
      }

      // declare the loser branch dead
      if (ma.hasLoserMatch())
      {
        declareLoserBranchDead(ma);
      }
    }
  }

  //----------------------------------------------------------------------------

  std::variant<OutgoingBracketLink, Rank> BracketMatchDataList::traverseForward(const BracketMatchData& ma, PairRole role) const
  {
    using bs = BracketMatchData::BranchState;

    // check whether we can start the traversal at all
    if ((role == PairRole::AsWinner) && !ma.hasWinnerMatch())
    {
      return ma.winnerRank().value();
    }
    if ((role == PairRole::AsLoser) && !ma.hasLoserMatch())
    {
      return ma.loserRank().value();
    }

    // initialize the traversal with the next match number,
    // based on whether we start as a winner or loser
    OutgoingBracketLink curMatchLink = (role == PairRole::AsWinner) ? ma.nextWinnerMatch() : ma.nextLoserMatch();

    // the iteration
    while (true)
    {
      const auto& curMatch = at(curMatchLink.dstMatch.get() - 1);  // only works because the list is SORTED by match number

      const auto& p1State = curMatch.pair1State();
      const auto& p2State = curMatch.pair2State();

      // both branches should NEVER be dead because then
      // we should have never reached them
      assert( ! ((p1State == bs::Dead) && (p2State == bs::Dead)));

      // this is a regular match that can be played
      if ((p1State != bs::Dead) && (p2State != bs::Dead))
      {
        return curMatchLink;
      }

      // one of the branches is dead and thus we traverse
      // in winner direction
      if (!curMatch.hasWinnerMatch())
      {
        return curMatch.winnerRank().value();
      }
      curMatchLink = curMatch.nextWinnerMatch();
    }
  }

  //----------------------------------------------------------------------------

  BracketMatchDataList convertToBracketMatches(const SvgBracketDef& def)
  {
    // create a merged list of all tags on all pages
    vector<TagData> allTags;
    for (const auto& pg : def.pages)
    {
      std::copy(begin(pg.rawTags), end(pg.rawTags), back_inserter(allTags));
    }

    // extract player and match data
    auto allPlayers = sortedPlayersFromTagList(allTags);
    auto allMatches = sortedMatchesFromTagList(allTags);

    // helper that determines where a winner / loser of a given match goes to
    auto findNextMatch = [&](const BracketMatchNumber& srcMatch, PairRole ro) -> std::optional<OutgoingBracketLink>
    {
      const int mNum = (ro == PairRole::AsWinner) ? srcMatch.get() : -srcMatch.get();

      auto it = find_if(allPlayers.begin(), allPlayers.end(), [&](const PlayerTag& p)
      {
        return (p.srcMatch == mNum);
      });

      if (it == end(allPlayers)) return std::optional<OutgoingBracketLink>{};

      return OutgoingBracketLink{BracketMatchNumber{it->bracketMatchNum}, it->playerPos};
    };

    // helper function that finds the player tag for a given
    // match and position
    auto findPlayerTag = [&](const BracketMatchNumber& ma, int pos) -> PlayerTag&
    {
      auto it = find_if(begin(allPlayers), end(allPlayers), [&](const PlayerTag& p)
      {
        return ((p.bracketMatchNum == ma.get()) && (p.playerPos == pos));
      });

      return *it;
    };

    // create the match data skeleton
    BracketMatchDataList result;
    std::transform(begin(allMatches), end(allMatches), back_inserter(result), [&](const MatchTag& m)
    {
      // basic match data
      BracketMatchNumber n{m.bracketMatchNum};
      Round r{m.roundNum};

      // what happens to the winner / loser
      std::variant<OutgoingBracketLink, Rank> winnerAction{Rank{-1}};  // dummy
      std::variant<OutgoingBracketLink, Rank> loserAction{Rank{-1}};  // dummy
      if (m.loserRank > 0)
      {
        loserAction = Rank{m.loserRank};
      } else {
        loserAction = findNextMatch(n, PairRole::AsLoser).value();
      }
      if (m.winnerRank > 0)
      {
        winnerAction = Rank{m.winnerRank};
      } else {
        winnerAction = findNextMatch(n, PairRole::AsWinner).value();
      }

      // find the player for this match
      auto p1 = findPlayerTag(n, 1);
      auto p2 = findPlayerTag(n, 2);

      // initial ranks in round 1
      if (m.roundNum == 1)
      {
        Rank ini1{Rank{p1.initialRank}};
        Rank ini2{Rank{p2.initialRank}};

        // insert a bracket match with initial ranks
        return BracketMatchData{n, ini1, ini2, winnerAction, loserAction};
      }

      // match in all rounds later than round 1
      const auto role1 = (p1.srcMatch > 0) ? PairRole::AsWinner : PairRole::AsLoser;
      const BracketMatchNumber srcMatch1{abs(p1.srcMatch.get())};
      const IncomingBracketLink in1{srcMatch1, role1};
      const auto role2 = (p2.srcMatch > 0) ? PairRole::AsWinner : PairRole::AsLoser;
      const BracketMatchNumber srcMatch2{abs(p2.srcMatch.get())};
      const IncomingBracketLink in2{srcMatch2, role1};

      return BracketMatchData{n, r, in1, in2, winnerAction, loserAction};
    });

    for (const auto& bmd : result)
    {
      cout << "#" << bmd.matchNum().get() << ": Round = " << bmd.round().get() << ", ";
      if (bmd.round().get() == 1)
      {
        cout << "P1.ini = " << bmd.initialRank(1).value().get() << ", P2.ini = " << bmd.initialRank(2).value().get() << ", ";
      } else {
        cout << "P1 = ";
        const auto in1 = bmd.inLink1();
        if (in1.role == SvgBracket::PairRole::AsWinner) cout << "W";
        else cout << "L";
        cout << in1.srcMatch.get() << ", ";

        cout << "P2 = ";
        const auto in2 = bmd.inLink2();
        if (in2.role == SvgBracket::PairRole::AsWinner) cout << "W";
        else cout << "L";
        cout << in2.srcMatch.get() << ", ";
      }

      if (bmd.winnerRank())
      {
        cout << "WR = " << bmd.winnerRank()->get() << ", ";
      } else {
        cout << "W --> ";
        const auto& out = bmd.nextWinnerMatch();
        cout << out.dstMatch.get() << "." << out.pos << ", ";
      }
      if (bmd.loserRank())
      {
        cout << "LR = " << bmd.loserRank()->get();
      } else {
        cout << "L --> ";
        const auto& out = bmd.nextLoserMatch();
        cout << out.dstMatch.get() << "." << out.pos;
      }

      cout << endl;
    }

    return result;
  }

}
