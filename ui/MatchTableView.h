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

#ifndef MATCHTABLEVIEW_H
#define	MATCHTABLEVIEW_H

#include <memory>

#include <QTableView>
#include <QSortFilterProxyModel>
#include <QListWidget>
#include <QMenu>
#include <QAction>
#include <QStringListModel>
#include <QTimer>

#include "TournamentDB.h"
#include "delegates/MatchItemDelegate.h"
#include "models/MatchTabModel.h"
#include "Match.h"
#include "AutoSizingTable.h"

using namespace QTournament;

class MatchTableView : public GuiHelpers::AutoSizingTableView_WithDatabase<QTournament::MatchTableModel>
{
  Q_OBJECT
  
public:
  MatchTableView (QWidget* parent);
  virtual ~MatchTableView () override;
  std::optional<QTournament::Match> getSelectedMatch() const;
  void updateSelectionAfterDataChange();
  void updateRefereeColumn();
  
protected:
  static constexpr int MaxNumericColWidth = 90;
  static constexpr int NumericColRelWidth = 8;
  static constexpr int CategoryColRelWidth = 9;
  static constexpr int MatchColRelWidth = 63;
  static constexpr int RefereeColRelWidth = 20;

  void hook_onDatabaseOpened() override;

private slots:
  void onSelectionChanged(const QItemSelection&selectedItem, const QItemSelection&deselectedItem);
  void onContextMenuRequested(const QPoint& pos);
  void onWalkoverP1Triggered();
  void onWalkoverP2Triggered();
  void onMatchDoubleClicked(const QModelIndex& index);
  void onAssignRefereeTriggered();
  void onRemoveRefereeTriggered();
  void onSectionHeaderDoubleClicked();
  void onMatchTimePredictionUpdate();
  void onMatchStatusChanged(int maId, int maSeqNum, ObjState oldStat, ObjState newStat);
  void onPrint1Selected();
  void onPrint4Selected();
  void onPrint8Selected();
  void onPrint12Selected();

signals:
  void matchSelectionChanged(int newlySelectedMatchId);

private:
  static constexpr int PredictionUpdateInterval_ms = 10 * 1000; // update every 10 secs
  MatchItemDelegate* matchItemDelegate;

  std::unique_ptr<QMenu> contextMenu;
  QAction* actPostponeMatch;
  QMenu* walkoverSelectionMenu;
  QMenu* courtSelectionMenu;
  QAction* actWalkoverP1;
  QAction* actWalkoverP2;

  QMenu* refereeMode_submenu;
  QAction* actAssignReferee;
  QAction* actRemoveReferee;

  QMenu* printSelectionMenu;
  QAction* printResultSheet1;
  QAction* printResultSheet4;
  QAction* printResultSheet8;
  QAction* printResultSheet12;

  std::unique_ptr<QTimer> predictionUpdateTimer;

  void initContextMenu();
  void updateContextMenu();
  void execWalkover(int playerNum);
  void showMatchBusyReason(const QTournament::Match& ma);
  void printResultSheets(int matchCount);
};

#endif	/* MATCHTABLEVIEW_H */

