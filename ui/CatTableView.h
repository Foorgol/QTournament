/*
 *    This is QTournament, a badminton tournament management program.
 *    Copyright (C) 2014 - 2015  Volker Knollmann
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

#ifndef CATTABLEVIEW_H
#define	CATTABLEVIEW_H

#include "Tournament.h"

#include <QTableView>

using namespace QTournament;

class CategoryTableView : public QTableView
{
  Q_OBJECT
  
public:
  CategoryTableView (QWidget* parent);
  virtual ~CategoryTableView ();
  bool isEmptyModel();
  Category getSelectedCategory();
  bool hasCategorySelected();
  
public slots:
  void onTournamentClosed();
  void onTournamentOpened(Tournament* tnmt);
  void onCategoryDoubleClicked(const QModelIndex& index);
  void onAddCategory();
  void onRemoveCategory();
  void onRunCategory();
  void onCloneCategory();
  void onAddPlayers();
  void onRemovePlayers();
  void onCreatePlayer();
  void onImportPlayer();
  
private slots:
  void onContextMenuRequested(const QPoint& pos);

private:
  Tournament* tnmt;
  QStringListModel* emptyModel;

  unique_ptr<QMenu> contextMenu;
  QAction* actAddCategory;
  QAction* actCloneCategory;
  QAction* actRunCategory;
  QAction* actRemoveCategory;
  QAction* actAddPlayer;
  QAction* actRemovePlayer;
  QAction* actCreateNewPlayerInCat;
  QAction* actImportPlayerToCat;

  void initContextMenu();

  void handleIntermediateSeedingForSelectedCat();
  bool unfreezeAndCleanup(unique_ptr<Category> selectedCat);

signals:
  void catModelChanged();

};

#endif	/* PLAYERTABLEVIEW_H */

