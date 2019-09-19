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

#include <QList>

#include "MatchResultList.h"
#include <SimpleReportGeneratorLib/SimpleReportGenerator.h>
#include <SimpleReportGeneratorLib/TableWriter.h>

#include "PlayerMngr.h"
#include "TeamMngr.h"
#include "Team.h"
#include "CatRoundStatus.h"
#include "ui/GuiHelpers.h"
#include "MatchMngr.h"
#include "MatchGroup.h"

namespace QTournament
{


MatchResultList::MatchResultList(const TournamentDB& _db, const QString& _name, const Category& _cat, int _round)
  :AbstractReport(_db, _name), cat(_cat), round(_round)
{
  // make sure that the requested round is already finished or at least running
  CatRoundStatus crs = cat.getRoundStatus();
  roundOffset = cat.getParameter_int(CatParameter::FirstRoundOffset);
  QList<int> runningRounds = crs.getCurrentlyRunningRoundNumbers();
  for (int runningRound : runningRounds)
  {
    if (round <= runningRound) return;   // okay, we are at least in one of the currently running rounds
  }

  if (round <= crs.getFinishedRoundsCount()) return; // okay, we're in one of the finished rounds

  throw std::runtime_error("Requested match results report for unfinished round.");
}

//----------------------------------------------------------------------------

upSimpleReport MatchResultList::regenerateReport()
{
  // collect the numbers of all match groups in this round
  MatchMngr mm{db};
  MatchGroupList mgl = mm.getMatchGroupsForCat(cat, round);
  if (mgl.size() > 1)
  {
    std::sort(mgl.begin(), mgl.end(), [](MatchGroup& mg1, MatchGroup& mg2){
      if (mg1.getGroupNumber() < mg2.getGroupNumber()) return true;
      return false;
    });
  }

  // prepare a subheader if we are in KO-rounds
  QString subHeader = QString();
  if ((mgl.size() == 1) && (mgl.at(0).getGroupNumber() > 0))
  {
    subHeader = GuiHelpers::groupNumToLongString(mgl.at(0).getGroupNumber());
  }

  upSimpleReport result = createEmptyReport_Portrait();
  QString repName = cat.getName() + tr(" -- Results of Round ") + QString::number(round + roundOffset);
  setHeaderAndHeadline(result.get(), repName, subHeader);

  // print a warning if the round is incomplete
  CatRoundStatus crs = cat.getRoundStatus();
  if (round > crs.getFinishedRoundsCount())
  {
    result->writeLine(tr("Note: the round is not finished yet; results are incomplete."), "", 1.0);
  }

  // print the results of each match group
  for (MatchGroup mg : mgl)
  {
    int grpNum = mg.getGroupNumber();

    // print a header if we are in round-robin rounds
    if (grpNum > 0)
    {
      printIntermediateHeader(result, GuiHelpers::groupNumToLongString(grpNum));
    }

    // print each finished match
    MatchList ml = mg.getMatches();
    std::sort(ml.begin(), ml.end(), [](Match& m1, Match& m2){
      return (m1.getMatchNumber() < m2.getMatchNumber());
    });
    printMatchList(result, ml, PlayerPairList(), GuiHelpers::groupNumToLongString(grpNum) + tr(" (cont.)"), true, false);
    result->skip(3.0);
  }

  // set header and footer
  setHeaderAndFooter(result, repName);

  return result;
}

//----------------------------------------------------------------------------

QStringList MatchResultList::getReportLocators() const
{
  QStringList result;

  QString loc = tr("Results::");
  loc += cat.getName() + tr("::by round::");
  loc += tr("Round ") + QString::number(round + roundOffset);

  result.append(loc);

  return result;
}

//----------------------------------------------------------------------------


//----------------------------------------------------------------------------


//----------------------------------------------------------------------------


//----------------------------------------------------------------------------


//----------------------------------------------------------------------------


}
