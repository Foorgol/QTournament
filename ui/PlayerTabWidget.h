/* 
 * File:   PlayerTabWidget.h
 * Author: volker
 *
 * Created on March 19, 2014, 7:38 PM
 */

#ifndef PLAYERTABWIDGET_H
#define	PLAYERTABWIDGET_H
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

#include "ui_PlayerTabWidget.h"

#include <QObject>
#include <QMenu>
#include <QAction>

#include "TournamentDB.h"

class PlayerTabWidget : public QWidget
{
  Q_OBJECT
public:
  PlayerTabWidget();
  virtual ~PlayerTabWidget ();
  void setDatabase(const QTournament::TournamentDB* _db);
  
private:
  const QTournament::TournamentDB* db{nullptr};
  Ui::PlayerTabWidget ui;

  std::unique_ptr<QMenu> registrationMenu;
  QAction* actRegisterAll;
  QAction* actUnregisterAll;

  std::unique_ptr<QMenu> extDatabaseMenu;
  QAction* actImportFromExtDatabase;
  QAction* actExportToExtDatabase;
  QAction* actSyncAllToExtDatabase;

  void initRegistrationMenu();
  void initExternalDatabaseMenu();

public slots:
  void onCreatePlayerClicked();
  void onPlayerDoubleClicked(const QModelIndex& index);
  void onPlayerCountChanged();
  void onRegisterAllTriggered();
  void onUnregisterAllTriggered();
  void onImportFromExtDatabase();
  void onExportToExtDatabase();
  void onSyncAllToExtDatabase();
  void onExternalDatabaseChanged();
  void onPlayerSelectionChanged(const QItemSelection &, const QItemSelection &);
  void onImportCsv();
};

#endif	/* _PLAYERTABWIDGET_H */
