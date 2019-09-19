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

#include <tuple>

#include <QList>

#include "MatrixAndStandings.h"
#include <SimpleReportGeneratorLib/SimpleReportGenerator.h>
#include <SimpleReportGeneratorLib/TableWriter.h>

#include "CatRoundStatus.h"
#include "ui/GuiHelpers.h"
#include "RankingMngr.h"
#include "RankingEntry.h"
#include "reports/commonReportElements/plotStandings.h"
#include "reports/commonReportElements/MatchMatrix.h"
#include "MatchMngr.h"
#include "PureRoundRobinCategory.h"

namespace QTournament
{


MatrixAndStandings::MatrixAndStandings(const TournamentDB& _db, const QString& _name, const Category& _cat, int _round)
  :AbstractReport(_db, _name), cat(_cat), round(_round)
{
  MatchSystem msys = cat.getMatchSystem();
  CatRoundStatus crs = cat.getRoundStatus();
  roundOffset = cat.getParameter_int(CatParameter::FirstRoundOffset);

  // make sure this category is eligible for a matrix view
  if ((msys != MatchSystem::GroupsWithKO) && (msys != MatchSystem::RoundRobin))
  {
    throw std::runtime_error("Requested matrix and standings report for invalid match type.");
  }

  // if we are in MatchSystem::GroupsWithKO make sure that "round" is still in
  // round-robin-phase
  if (msys == MatchSystem::GroupsWithKO)
  {
    KO_Config cfg = KO_Config(cat.getParameter_string(CatParameter::GroupConfig));
    int numGroupRounds = cfg.getNumRounds();
    if (round > numGroupRounds)
    {
      throw std::runtime_error("Requested matrix and standings report for elimination phase of category.");
    }
  }

  if (round <= 0)
  {
    return;   // matrix for initial matches
  }

  // make sure that the requested round is already finished
  if (round > crs.getFinishedRoundsCount())
  {
    throw std::runtime_error("Requested matrix and standings report for unfinished round.");
  }

  // make the round is valid at all
  if (round < 0)
  {
    throw std::runtime_error("Requested matrix and standings report for invalid round.");
  }
}

//----------------------------------------------------------------------------

upSimpleReport MatrixAndStandings::regenerateReport()
{
  // retrieve the ranking(s) for this round
  RankingMngr rm{db};
  RankingEntryListList rll;
  if (round > 0) rll = rm.getSortedRanking(cat, round);
  if (round < 0) rll = rm.getSortedRanking(cat, -round - 1);   // see below for the bad hack about negative round numbers

  // if we are in round robins with multiple iterations,
  // create a subhead indicating the current iteration number
  QString subHead;
  MatchSystem msys = cat.getMatchSystem();
  int curIteration = -1;
  if (msys == MatchSystem::RoundRobin)
  {
    PureRoundRobinCategory rrCat{db, cat.rowRef()};
    if (rrCat.getIterationCount() > 1)
    {
      int rpi = rrCat.getRoundCountPerIteration();
      curIteration = (abs(round) - 1) / rpi;  // will be >= 0 even if round==0
      ++curIteration;
      subHead = tr("%1. Iteration");
      subHead = subHead.arg(curIteration);
    }
  }

  // determine the report name / headline
  QString repName = cat.getName() + " -- ";
  if (round <= 0)
  {
    repName += tr("Initial matches");
    if (curIteration > 0)
    {
      repName += tr(", %1. Iteration");
      repName = repName.arg(curIteration);
      subHead.clear();
    }
  } else {
    repName += tr("Match matrix and standings after round ") + QString::number(round + roundOffset);
  }

  upSimpleReport result = createEmptyReport_Portrait();
  setHeaderAndHeadline(result.get(), repName, subHead);
  result->skip((round <= 0) ? 15 : 5);

  // determine the number of match groups
  int nGroups = 1;  // round robin
  if (msys == MatchSystem::GroupsWithKO)
  {
    KO_Config cfg{cat.getParameter_string(CatParameter::GroupConfig)};
    nGroups = cfg.getNumGroups();
  }

  // plot the matrix and, if applicable, the standings
  int cnt=0;
  QString catName = cat.getName();
  for (int grpNum = 1; grpNum <= nGroups; ++grpNum)
  {
    QString tableName = catName;
    if (msys == MatchSystem::GroupsWithKO)
    {
      tableName = tr("Group ") + QString::number(grpNum);
    }

    // plot the matrix
    MatchMatrix matrix{result.get(), tableName, cat, round, (msys == MatchSystem::RoundRobin) ? -1 : grpNum};
    auto plotRect = matrix.plot();
    result->skip(plotRect.size().height() + 3.0);

    // plot the standings, if available
    if (round != 0)
    {
      if (rll.empty() || ((rll.size() == 1) && (rll.at(0).size() == 0)))
      {
        result->writeLine(tr("There are no standings for this round yet."));
      } else {
        // the ranking for this group and print it
        for (const RankingEntryList& rl : rll)
        {
          // skip empty entries (there shouldn't be any, but anyway...)
          if (rl.empty()) continue;

          // skip entries belong to the wrong group
          if ((msys == MatchSystem::GroupsWithKO) && (rl.at(0).getGroupNumber() != grpNum)) continue;

          // okay, we found the right entry
          plotStandings elem{result.get(), rl, tableName};
          elem.plot();
        }
      }
    }

    // start a new page after every second matrix
    if ((cnt % 2) == 0)
    {
      result->skip((round <= 0) ? 30 : 10);
    } else if (cnt < (nGroups - 1)){
      result->startNextPage();
      result->skip(10);
    }

    ++cnt;
  }

  // set header and footer
  setHeaderAndFooter(result, repName);

  return result;
}

//----------------------------------------------------------------------------

QStringList MatrixAndStandings::getReportLocators() const
{
  QStringList result;

  QString loc = tr("Matrix and Standings::");
  loc += cat.getName() + "::";

  // insert the number of the iteration, if applicable
  MatchSystem msys = cat.getMatchSystem();
  if (msys == MatchSystem::RoundRobin)
  {
    PureRoundRobinCategory rrCat{db, cat.rowRef()};
    // if we play more than one iteration, add another
    // location "sub-tree" for the iteration number
    if (rrCat.getIterationCount() > 1)
    {
      int rpi = rrCat.getRoundCountPerIteration();
      int curIteration = (abs(round) - 1) / rpi;  // will be >= 0 even if round==0
      loc += tr("%1. Iteration::");
      loc = loc.arg(curIteration + 1);
    }
  }

  if (round > 0)
  {
    loc += tr("after round ") + QString::number(round + roundOffset);
  } else {
    loc += tr("initial matches");
  }

  result.append(loc);

  return result;
}

//----------------------------------------------------------------------------


//----------------------------------------------------------------------------


//----------------------------------------------------------------------------


//----------------------------------------------------------------------------


//----------------------------------------------------------------------------


}
