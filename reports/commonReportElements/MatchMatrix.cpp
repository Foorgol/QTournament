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

#include <SimpleReportGeneratorLib/TableWriter.h>

#include "MatchMatrix.h"
#include "MatchMngr.h"
#include "CatRoundStatus.h"
#include "TournamentDB.h"
#include "reports/AbstractReport.h"
#include "PureRoundRobinCategory.h"

using namespace SimpleReportLib;
using namespace QTournament;

MatchMatrix::MatchMatrix(SimpleReportGenerator* _rep, const QString& tabName, const Category& _cat, int _round, int _grpNum)
  :AbstractReportElement(_rep), tableName(tabName), cat(_cat), round(_round), grpNum(_grpNum), showMatchNumbersOnly(round <= 0)
{
  MatchSystem msys = cat.getMatchSystem();

  if ((msys != MatchSystem::RoundRobin) && (round < 0))
  {
    throw invalid_argument("Requested match matrix for invalid round number (too low)");
  }

  if ((msys != MatchSystem::RoundRobin) && (msys != MatchSystem::GroupsWithKO))
  {
    throw invalid_argument("Requested match matrix for invalid category (wrong match system)");
  }

  if (msys == MatchSystem::GroupsWithKO)
  {
    KO_Config cfg{cat.getParameter_string(CatParameter::GroupConfig)};
    if (round > cfg.getNumRounds())
    {
      throw invalid_argument("Requested match matrix a non-round-robin round!");
    }

    if ((grpNum < 1) || (grpNum > cfg.getNumGroups()))
    {
      throw invalid_argument("Requested match matrix an invalid group number");
    }
  } else {
    if (grpNum != -1)   // grpNum must be -1 for round robin categories
    {
      throw invalid_argument("Requested match matrix with an invalid group number");
    }
  }

  if (round <= 0)
  {
    if (msys != MatchSystem::RoundRobin)
    {
      round = 0;    // just a caveat, should actually never be reached
    }

    round = -round;

    return;   // matrix for initial matches
  }

  CatRoundStatus crs = cat.getRoundStatus();
  if (round > crs.getFinishedRoundsCount())
  {
    throw invalid_argument("Requested match matrix for invalid round number (too high)");
  }
}

//----------------------------------------------------------------------------

QRectF MatchMatrix::plot(const QPointF& topLeft)
{
  QPointF origin = topLeft;

  if ((topLeft.x() < 0) && (topLeft.y() < 0))
  {
    origin = rep->getAbsCursorPos();
  }

  // get the player names for this group
  PlayerPairList ppList;
  ppList = cat.getPlayerPairs(grpNum);

  // determine the maximum round number up to
  // which will be searched for matches
  int maxRoundNum = 99999;  // default: search in whole category
  MatchSystem msys = cat.getMatchSystem();
  if (msys == MatchSystem::GroupsWithKO)
  {
    KO_Config cfg{cat.getParameter_string(CatParameter::GroupConfig)};

    // in group matches, limit the search radius to the
    // group phase, because otherwise we might end up displaying
    // matches from the KO phase
    maxRoundNum = cfg.getNumRounds();
  }

  // determine the minimum round number from that on
  // we will search for matches.
  // This offset is needed when playing multiple round robin iterations
  //
  // also update maxRoundNum as the upper limit of the search radius
  int minRoundNum = 1;
  if ((msys == MatchSystem::RoundRobin) && (round > 0))
  {
    auto rrCat_ = cat.convertToSpecializedObject();
    PureRoundRobinCategory* rrCat = dynamic_cast<PureRoundRobinCategory*>(rrCat_.get());
    int rpi = rrCat->getRoundCountPerIteration();
    int curIteration = (round - 1) / rpi;  // will be >= 0 even if round==0
    minRoundNum = curIteration * rpi + 1;
    maxRoundNum = (curIteration + 1) * rpi;
  }

  // get the textstyle for the table contents
  TextStyle* baseStyle = rep->getTextStyle();
  assert(baseStyle != nullptr);
  TextStyle* boldStyle = rep->getTextStyle(AbstractReport::BoldStyle);
  assert(boldStyle != nullptr);

  // prepare the grid
  int nPlayers = ppList.size();
  MatrixGrid grid{origin, rep->getUsablePageWidth(), nPlayers + 1, 6 * baseStyle->getFontSize_MM()};

  // plot the grid
  for (int r=0; r < (nPlayers+1); ++r)
  {
    for (int c=0; c < (nPlayers+1); ++c)
    {
      auto cell = grid.getCell(r, c);

      if ((r == c) && (r != 0))
      {
        rep->drawRect(cell, MED, QColor(Qt::lightGray));
      } else {
        rep->drawRect(cell);
      }
    }
  }

  // plot the grid contents
  for (int r=0; r < (nPlayers+1); ++r)
  {
    for (int c=0; c < (nPlayers+1); ++c)
    {
      // get the cell's content
      QString txt;
      CellContentType cct;
      tie(cct, txt) = getCellContent(ppList, r, c, minRoundNum, maxRoundNum);

      // special case: content == tableName
      if (cct == CellContentType::Title)
      {
        txt = tableName;
      }

      // determine the font for the cell content
      TextStyle* style = (cct == CellContentType::Title) ? boldStyle : baseStyle;

      // determine the position of the content within the cell
      // and the content's reference position (text base point)
      RECT_CORNER posInCell = RECT_CORNER::CENTER;
      RECT_CORNER txtBasePoint = RECT_CORNER::CENTER;
      if (cct == CellContentType::MatchScore)
      {
        posInCell = RECT_CORNER::TOP_RIGHT;
        txtBasePoint = RECT_CORNER::TOP_RIGHT;
      }

      // determine the cell's size and position
      auto cell = grid.getCell(r, c);
      if (cct == CellContentType::MatchScore)
      {
        // shrink the cell to virtually add some margin
        // between match number and grid line
        cell.adjust(0, GapTextToGrid_mm, -GapTextToGrid_mm, 0);
      }

      // prepare the player names for multiline headers
      if (cct == CellContentType::Header)
      {
        PlayerPair pp = ppList.at(r + c - 1);  // either r or c is guaranteed to be 0
        txt = getTruncatedPlayerNames(pp, style, cell.width() - 2 * GapTextToGrid_mm);
      }

      // plot the contents. Call the "multiline"-function even although we
      // sometime have only one line of text. That doesn't do any harm-.
      rep->drawMultilineText(cell, posInCell, txtBasePoint, txt, CENTER, 0.15 * style->getFontSize_MM(), style);

      // draw the "mirrored" match result, if applicable
      //
      // this string-based approach is faster than calling again
      // getMatchForCell() with swapped row/column values
      if (cct == CellContentType::Score)
      {
        QString swappedScore;
        for (QString gameScore : txt.split("\n"))
        {
          gameScore = gameScore.trimmed();
          QStringList scores = gameScore.split(" : ");
          if (scores.length() > 1)
          {
            QString sc1 = scores.at(0);
            QString sc2 = scores.at(1);
            if (sc2.endsWith(",")) sc2.chop(1);

            QString s = "%1 : %2\n";
            swappedScore += s.arg(sc2).arg(sc1);
          } else {
            swappedScore += gameScore + "\n";  // in this case, the value of "gamescore" is "walkover";
          }
        }
        swappedScore.chop(1);
        auto mirroredCell = grid.getCell(c, r);
        rep->drawMultilineText(mirroredCell, posInCell, txtBasePoint, swappedScore, CENTER, 0.15 * style->getFontSize_MM(), style);
      }

      // draw the "mirrored" match number
      if (cct == CellContentType::MatchScore)
      {
        auto mirroredCell = grid.getCell(c, r);
        mirroredCell.adjust(0, GapTextToGrid_mm, -GapTextToGrid_mm, 0);
        rep->drawText(mirroredCell, posInCell, txtBasePoint, txt, style);
      }
    }
  }

  return grid.getGridRect(nPlayers + 1);
}

//----------------------------------------------------------------------------

std::optional<QTournament::Match> MatchMatrix::getMatchForCell(const PlayerPairList& ppList, int row, int col, int minRound, int maxRound) const
{
  if ((row < 1) || (col < 1) || (row > ppList.size()) || (col > ppList.size()))
  {
    return {};
  }

  PlayerPair ppRow = ppList.at(row - 1);
  PlayerPair ppCol = ppList.at(col - 1);

  // create a direct, low-level database query for the
  // applicable matches
  auto& db = cat.getDatabaseHandle();
  MatchMngr mm{db};
  Sloppy::estring where = "(%1 = %2 AND %3 = %4) OR (%1 = %4 AND %3 = %2)";
  where.arg(MA_Pair1Ref);
  where.arg(ppRow.getPairId());
  where.arg(MA_Pair2Ref);
  where.arg(ppCol.getPairId());
  SqliteOverlay::DbTab matchTab{db, TabMatch, false};

  for (const auto& row : matchTab.getRowsByWhereClause(where))
  {
    auto ma = mm.getMatch(row.id());
    assert(ma);

    // check for right category and the right round number
    int r = ma->getMatchGroup().getRound();
    if ((ma->getCategory() == cat) && (r >= minRound) && (r <= maxRound))
    {
      return ma;
    }
  }

  return {};
}

//----------------------------------------------------------------------------

QStringList MatchMatrix::getSortedMatchScoreStrings(const Match& ma, const PlayerPair& ppRow, const PlayerPair& ppCol) const
{
  PlayerPair pp1 = ma.getPlayerPair1();
  PlayerPair pp2 = ma.getPlayerPair2();
  if (!(  ((pp1 == ppRow) && (pp2 == ppCol)) || ((pp2 == ppRow) && (pp1 == ppCol))  ) )
  {
    return QStringList();
  }

  bool mustSwapScore = (ppRow == pp2);

  auto score = ma.getScore();
  if (!score) return QStringList();

  QStringList result;
  for (int g=0; g < score->getNumGames(); ++g)
  {
    auto gameScore = score->getGame(g);
    assert(gameScore);

    int sc1;
    int sc2;
    tie(sc1, sc2) = gameScore->getScore();

    if (mustSwapScore)
    {
      int tmp = sc1;
      sc1 = sc2;
      sc2 = tmp;
    }

    QString s = "%1 : %2";
    s = s.arg(sc1).arg(sc2);
    result.append(s);
  }

  return result;
}

//----------------------------------------------------------------------------

tuple<MatchMatrix::CellContentType, QString> MatchMatrix::getCellContent(const PlayerPairList& ppList, int row, int col, int minRound, int maxRound) const
{
  // the table name goes in the top-left corner
  if ((row == 0) && (col == 0))
  {
    return make_tuple(CellContentType::Title, QString());  // don't return the title itself, it's already know to the caller
  }

  // the player names go to column 0 and row 0
  if ((row == 0) || (col == 0))
  {
    return make_tuple(CellContentType::Header, ppList.at(row + col - 1).getDisplayName());
  }

  // for every cell above the diagonal, check for
  // the associated match and print the score
  // or the match number, if applicable
  if ((row > 0) && (col > row))
  {
    auto ma = getMatchForCell(ppList, row, col, minRound, maxRound);

    if (!ma)
    {
      return make_tuple(CellContentType::Empty, QString());
    }

    int maRound = ma->getMatchGroup().getRound();
    ObjState maStat = ma->getState();

    // if the match is later than "round", print only
    // the match number. The same applies if the match
    // is not yet finished
    if ((maRound > round) || (maStat != ObjState::MA_Finished) || (showMatchNumbersOnly))
    {
      int maNum = ma->getMatchNumber();
      if (maNum < 0)
      {
        // no score, no match number. nothing more to do.
        return make_tuple(CellContentType::Empty, QString());
      }

      // the cell content will be the match number
      QString txt = "#" + QString::number(maNum);
      return make_tuple(CellContentType::MatchScore, txt);
    }

    if ((maRound <= round) && (maStat == ObjState::MA_Finished))
    {
      // the match is in the correct round range and is finished,
      // so we print the score
      PlayerPair ppRow = ppList.at(row - 1);
      PlayerPair ppCol = ppList.at(col - 1);
      QStringList scList = getSortedMatchScoreStrings(*ma, ppRow, ppCol);

      QString txt;
      for (const QString& l : scList)
      {
        txt += l + "\n";
      }
      txt.chop(1);

      // add a line "walkover", if necessary
      if (ma->isWonByWalkover())
      {
        txt += "\n" + tr("walkover");
      }

      return make_tuple(CellContentType::Score, txt);
    }
  }

  return make_tuple(CellContentType::Empty, QString());
}

//----------------------------------------------------------------------------

QString MatchMatrix::getTruncatedPlayerNames(const PlayerPair& pp, const TextStyle* style, double maxWidth) const
{
  // a little helper function that truncates a string
  // until it fits to a maximum width
  auto truncStringToWidth = [&](const QString& src, const QString& postfix) {
    int fullLen = src.length();
    QString truncString = src;
    for (int len = fullLen; len > 3; --len)  // keep at least three characters
    {
      double width = rep->getTextDimensions_MM(truncString + postfix, style).width();
      if (width <= maxWidth) break;
      truncString.chop(1);
    }

    return truncString + postfix;
  };

  // a little helper function that truncates a player name
  // until it fits to a maximum width
  auto truncPlayerName = [&](const Player& _p, const QString& postfix) {
    int fullLen = _p.getDisplayName().length();
    QString truncName;
    for (int len = fullLen; len > 3; --len)
    {
      truncName = _p.getDisplayName(len) + postfix;
      double width = rep->getTextDimensions_MM(truncName, style).width();
      if (width <= maxWidth) break;
    }

    return truncName;
  };

  QString result;
  if (pp.hasPlayer2())
  {
    // doubles or mixed; one full name per line
    result = truncPlayerName(pp.getPlayer1(), " /");
    result += "\n";
    result += truncPlayerName(pp.getPlayer2(), QString());
  } else {
    // singles; one line for last name, one for first name
    Player p = pp.getPlayer1();
    result = truncStringToWidth(p.getLastName(), ",");
    result += "\n";
    result += truncStringToWidth(p.getFirstName(), QString());
  }

  return result;
}

//----------------------------------------------------------------------------
//----------------------------------------------------------------------------
//----------------------------------------------------------------------------
//----------------------------------------------------------------------------

MatrixGrid::MatrixGrid(double _width, int _nCols, double _rowHeight)
  :MatrixGrid(QPointF(0,0), _width, _nCols, _rowHeight)
{

}

//----------------------------------------------------------------------------

MatrixGrid::MatrixGrid(const QPointF& _topLeft, double _width, int _nCols, double _rowHeight)
  :topLeft(_topLeft), width(_width), nCols(_nCols), rowHeight(_rowHeight)
{
  if (width <= 0)
  {
    throw invalid_argument("Can't have negative or zero grid width");
  }

  if (nCols <= 0)
  {
    throw invalid_argument("Can't have negative or zero number of grid columns");
  }

  if (rowHeight < 0)
  {
    throw invalid_argument("Can't have negative or zero grid row height.");
  }

  colWidth = width / nCols;
  cellSize = QSizeF(colWidth, rowHeight);
}

//----------------------------------------------------------------------------

QRectF MatrixGrid::getCell(int row, int col) const
{
  if ((row < 0) || (col < 0)) return QRectF();

  QPointF cellTopLeft = topLeft + QPointF(colWidth * col, rowHeight * row);

  return QRectF(cellTopLeft, cellSize);
}

//----------------------------------------------------------------------------

QSizeF MatrixGrid::getGridSize(int nRows) const
{
  if (nRows < 1) return QSizeF();

  return QSizeF(colWidth * nCols, nRows * rowHeight);
}

//----------------------------------------------------------------------------

QRectF MatrixGrid::getGridRect(int nRows) const
{
  if (nRows < 1) return QRectF();

  return QRectF(topLeft, getGridSize(nRows));
}

//----------------------------------------------------------------------------

QRectF MatrixGrid::getGridRegion(int startRow, int startCol, int rowSpan, int colSpan) const
{
  if ((startRow < 0) || (startCol < 0) || (rowSpan < 1) || (colSpan < 1))
  {
    return QRectF();
  }

  auto startCell = getCell(startRow, startCol);

  return QRectF(startCell.topLeft(), QSize(colSpan * colWidth, rowSpan * rowHeight));
}
