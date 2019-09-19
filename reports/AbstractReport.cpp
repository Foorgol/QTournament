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

#include <QObject>

#include <SimpleReportGeneratorLib/SimpleReportGenerator.h>
#include <SimpleReportGeneratorLib/TableWriter.h>

#include "AbstractReport.h"
#include "HelperFunc.h"

using namespace SimpleReportLib;
using namespace QTournament;

namespace QTournament
{

  constexpr char AbstractReport::HeadlineStyle[];
  constexpr char AbstractReport::SubheadStyle[];
  constexpr char AbstractReport::IntermediateHeadlineStyle[];
  constexpr char AbstractReport::ResultsheetNameStyle[];
  constexpr char AbstractReport::ResultsheetTeamStyle[];
  constexpr char AbstractReport::ResultsheetGamelabelStyle[];
  constexpr char AbstractReport::BoldStyle[];

AbstractReport::AbstractReport(const TournamentDB& _db, const QString& _name)
  :db(_db), name(_name), cfg(SqliteOverlay::KeyValueTab{db, TabCfg})
{
  if (name.isEmpty() || (name == ""))
  {
    throw std::runtime_error("Got empty name for report");
  }
}

//----------------------------------------------------------------------------

AbstractReport::~AbstractReport()
{

}

//----------------------------------------------------------------------------

upSimpleReport AbstractReport::createEmptyReport_Portrait() const
{
  SimpleReportLib::SimpleReportGenerator* rawResult = new SimpleReportLib::SimpleReportGenerator(
        WidthA4_mm, HeightA4_mm, DefaultMargin_mm);
  rawResult->startNextPage();

  upSimpleReport result = upSimpleReport(rawResult);
  prepStyles(result);

  return result;
}

//----------------------------------------------------------------------------

upSimpleReport AbstractReport::createEmptyReport_Landscape() const
{
  SimpleReportLib::SimpleReportGenerator* rawResult = new SimpleReportLib::SimpleReportGenerator(
        HeightA4_mm, WidthA4_mm, DefaultMargin_mm);
  rawResult->startNextPage();

  upSimpleReport result = upSimpleReport(rawResult);
  prepStyles(result);

  return result;
}

//----------------------------------------------------------------------------

QString AbstractReport::getName() const
{
  return name;
}

//----------------------------------------------------------------------------

void AbstractReport::setHeaderAndHeadline(SimpleReportLib::SimpleReportGenerator* rep, const QString& headline, const QString& subHead) const
{
  SimpleReportLib::TabSet ts;
  ts.addTab(WidthA4_mm / 2.0 - DefaultMargin_mm, SimpleReportLib::TAB_JUSTIFICATION::TAB_CENTER);
  rep->pushTabs(ts);
  rep->writeLine("\t" + headline, HeadlineStyle, 0.0, BeforeHeadlineSkip_mm);
  if (subHead.length() > 0)
  {
    rep->skip(HeadSubheadSkip_mm);
    rep->writeLine("\t" + subHead, SubheadStyle);
  }
  rep->popTabs();
  rep->skip(AfterHeadlineSkip_mm);
}

//----------------------------------------------------------------------------

void AbstractReport::prepStyles(upSimpleReport& rep) const
{
  // basic font: 2 mm Arial
  auto style = rep->getTextStyle();
  style->setFontname(QLatin1Literal("Arial"));
  style->setFontSize_MM(2.0);

  // bold font
  style = rep->createChildTextStyle(BoldStyle);
  style->setBoldState(true);

  // headlines: 4 mm, bold
  style = rep->createChildTextStyle(HeadlineStyle);
  style->setFontSize_MM(4.0);
  style->setBoldState(true);

  // sub-heaadline: 2,5 mm, italic
  style = rep->createChildTextStyle(SubheadStyle);
  style->setFontSize_MM(2.5);
  style->setItalicsState(true);

  // intermediate headine: 3,5 mm, not bold
  style = rep->createChildTextStyle(IntermediateHeadlineStyle);
  style->setFontSize_MM(3.5);

  // player names in result sheets: 3 mm, bold
  style = rep->createChildTextStyle(ResultsheetNameStyle);
  style->setFontSize_MM(2.9);
  style->setBoldState(true);

  // team names in result sheets: 2 mm, italic
  style = rep->createChildTextStyle(ResultsheetTeamStyle);
  style->setFontSize_MM(2.0);
  style->setItalicsState(true);

  // "Game xx:"-labels in result sheets: 2,5 mm, bold
  style = rep->createChildTextStyle(ResultsheetGamelabelStyle);
  style->setFontSize_MM(2.5);
  style->setBoldState(true);
}

//----------------------------------------------------------------------------

void AbstractReport::printIntermediateHeader(upSimpleReport& rep, const QString& txt, double skipBefore__MM) const
{
  if (skipBefore__MM < 0) skipBefore__MM = 0.0;

  // check for sufficient space
  if (!(rep->hasSpaceForAnotherLine(IntermediateHeadlineStyle, skipBefore__MM + SkipAfterIntermediateHeader_mm)))
  {
    rep->startNextPage();
  }
  if (skipBefore__MM > 0) rep->skip(skipBefore__MM);
  rep->writeLine(txt, IntermediateHeadlineStyle);
  rep->addHorLine();
  rep->skip(SkipAfterIntermediateHeader_mm);
}

//----------------------------------------------------------------------------

void AbstractReport::printMatchList(upSimpleReport& rep, const MatchList& maList, const PlayerPairList& byeList, const QString& continuationString, bool withResults, bool withGroupColumn) const
{
  // prepare a tabset for a table with match results
  SimpleReportLib::TabSet ts;
  ts.addTab(9.5, SimpleReportLib::TAB_JUSTIFICATION::TAB_RIGHT);   // the match number
  ts.addTab(13.0, SimpleReportLib::TAB_JUSTIFICATION::TAB_LEFT);   // the players
  ts.addTab(135.0, SimpleReportLib::TAB_JUSTIFICATION::TAB_CENTER);   // the group number
  ts.addTab(145, SimpleReportLib::TAB_JUSTIFICATION::TAB_LEFT);   // dummy tab for a column label
  for (int game=0; game < 5; ++game)
  {
    double colonPos = 150 + game*12.0;
    ts.addTab(colonPos - 1.0,  SimpleReportLib::TAB_JUSTIFICATION::TAB_RIGHT);  // first score
    ts.addTab(colonPos,  SimpleReportLib::TAB_JUSTIFICATION::TAB_CENTER);  // colon
    ts.addTab(colonPos + 1.0,  SimpleReportLib::TAB_JUSTIFICATION::TAB_LEFT);  // second score
  }

  // prepare a table for the match results
  SimpleReportLib::TableWriter tw(ts);
  tw.setHeader(1, QObject::tr("Match"));
  tw.setHeader(2, QObject::tr("Players"));
  if (withResults)
  {
    tw.setHeader(4, QObject::tr("Game results"));
  }

  // print the results of all matches
  bool usesRoundRobinGroups = false;
  for (Match ma : maList)
  {
    int grpNum = ma.getMatchGroup().getGroupNumber();
    if (grpNum > 0)
    {
      usesRoundRobinGroups = true;
    }

    QStringList rowContent;
    rowContent << "";  // first column not used
    rowContent << QString::number(ma.getMatchNumber());

    QString pNames;
    if (ma.hasPlayerPair1())
    {
      pNames = ma.getPlayerPair1().getDisplayName();
    } else {
      // the match is not yet complete
      // and thus can't be displayed in the report
      continue;
    }

    pNames += "   :   ";

    if (ma.hasPlayerPair2())
    {
      pNames += ma.getPlayerPair2().getDisplayName();
    } else {
      // the match is not yet complete
      // and thus can't be displayed in the report
      continue;
    }

    rowContent << pNames;

    if (withGroupColumn && (grpNum > 0))
    {
      rowContent << QString::number(grpNum);
    } else {
      rowContent << "";
    }

    rowContent << "";  // a dummy tab for the "Game results" label

    if (withResults)
    {
      auto ms = ma.getScore();
      if (!ms) continue;
      for (int i=0; i < ms->getNumGames(); ++i)
      {
        auto gs = ms->getGame(i);
        assert(gs);

        std::tuple<int, int> sc = gs->getScore();
        rowContent << QString::number(get<0>(sc));
        rowContent << ":";
        rowContent << QString::number(get<1>(sc));
      }

      // if this was a walkover, add a note in
      // the unused space for the 3rd game
      if (ma.isWonByWalkover())
      {
        rowContent << "";
        rowContent << QObject::tr("(walkover)");
      }
    }

    tw.appendRow(rowContent);
  }

  // if used we round robins, add a column header for the group number
  if (usesRoundRobinGroups && withGroupColumn)
  {
    tw.setHeader(3, QObject::tr("Group"));
  }

  // if we have players with a bye, list those players at the bottom of the table
  if (!(byeList.empty()))
  {
    // sort the list alphabetically (and not, e.g., by group)
    //
    // we need to do this on a copy of byeList, because byeList is const
    // and making it non-const would cause trouble when calling this function
    // with an empty list like "PlayerPairList()".
    PlayerPairList _byeList{byeList};
    std::sort(_byeList.begin(), _byeList.end(), [](PlayerPair& pp1, PlayerPair& pp2)
    {
      return pp1.getDisplayName() < pp2.getDisplayName();
    });

    // print two empty rows as delimiter
    QStringList rowContent;
    rowContent << "";
    tw.appendRow(rowContent);
    tw.appendRow(rowContent);

    // a list of all players having a bye
    for (PlayerPair pp : _byeList)
    {
      rowContent.clear();
      rowContent << "";  // first column not used
      rowContent << "---";  // indicates: no match / no match number ==> bye

      rowContent << pp.getDisplayName();
      tw.appendRow(rowContent);
    }
  }

  tw.setNextPageContinuationCaption(continuationString);
  tw.write(rep.get());
}

//----------------------------------------------------------------------------

void AbstractReport::setHeaderAndFooter(upSimpleReport& rep, const QString& reportName)
{
  QString tName = stdString2QString(cfg[CfgKey_TnmtName]);
  QString cName = stdString2QString(cfg[CfgKey_TnmtOrga]);
  rep->setGlobalHeader(cName, tName, SimpleReportLib::HeaderFooterStrings::TOKEN_CURDATE);

  QString fl = SimpleReportLib::HeaderFooterStrings::TOKEN_CURTIME;

  QString fr = SimpleReportLib::HeaderFooterStrings::TOKEN_CURPGNUM;
  fr += " / ";
  fr += SimpleReportLib::HeaderFooterStrings::TOKEN_TOTALPGNUM;
  rep->setGlobalFooter(fl , reportName, fr);

  rep->applyHeaderAndFooterOnAllPages();
}

//----------------------------------------------------------------------------


//----------------------------------------------------------------------------


}
