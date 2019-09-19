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

#ifndef EXTERNALPLAYERDB_H
#define	EXTERNALPLAYERDB_H

#include <memory>
#include <tuple>
#include <functional>

#include <QHash>

#include <SqliteOverlay/SqliteDatabase.h>
#include <SqliteOverlay/DbTab.h>
#include <SqliteOverlay/TabRow.h>

#include "TournamentDataDefs.h"

namespace QTournament
{

#define EXT_PLAYER_DB_VERSION 1
#define MIN_REQUIRED_EXT_PLAYER_DB_VERSION 1
#define TAB_EPD_CFG std::string("Config")
#define CFG_KEY_EPD_DB_VERSION std::string("DatabaseVersion")

#define TAB_EPD_PLAYER std::string("Player")
#define EPD_PL_Fname std::string("FirstName")
#define EPD_PL_Lname std::string("LastName")
#define EPD_PL_Sex std::string("Sex")

  class ExternalPlayerDatabaseEntry
  {
  public:
    ExternalPlayerDatabaseEntry(int _id, const QString& _fname, const QString& _lname, Sex _sex = Sex::DontCare);
    ExternalPlayerDatabaseEntry(const QString& _fname, const QString& _lname, Sex _sex = Sex::DontCare);
    QString getLastname() const;
    QString getFirstname() const;
    QString getDisplayName() const;
    inline int getId() const { return id; }
    inline Sex getSex() const { return sex; }

  protected:
    int id;
    QString fName;
    QString lName;
    Sex sex;
    static std::function<bool (ExternalPlayerDatabaseEntry&, ExternalPlayerDatabaseEntry&)> getPlayerSortFunction_byName();
  };
  using opExternalPlayerDatabaseEntry = std::optional<ExternalPlayerDatabaseEntry>;
  using ExternalPlayerDatabaseEntryList = QList<ExternalPlayerDatabaseEntry>;

  //----------------------------------------------------------------------------

  class ExternalPlayerDB : public SqliteOverlay::SqliteDatabase
  {
    friend class SqliteDatabase;

  public:
    static std::optional<ExternalPlayerDB> createNew(const QString& fname);
    static std::optional<ExternalPlayerDB> openExisting(const QString& fname);
    virtual void populateTables();
    virtual void populateViews();

    ExternalPlayerDatabaseEntryList searchForMatchingPlayers(const QString& substring);
    ExternalPlayerDatabaseEntryList getAllPlayers();
    opExternalPlayerDatabaseEntry getPlayer(int id);
    opExternalPlayerDatabaseEntry getPlayer(const QString& fname, const QString& lname);
    opExternalPlayerDatabaseEntry storeNewPlayer(const ExternalPlayerDatabaseEntry& newPlayer);
    bool hasPlayer(const QString& fname, const QString& lname);
    bool updatePlayerSexIfUndefined(int extPlayerId, Sex newSex);
    std::tuple<QList<int>, QList<int>, QHash<int, QString>, int> bulkImportCSV(const QString& csv);

  private:
    opExternalPlayerDatabaseEntry row2upEntry(const SqliteOverlay::TabRow& r) const;
    ExternalPlayerDB(const std::string& fname, SqliteOverlay::OpenMode om);
  };

  using upExternalPlayerDB = std::unique_ptr<ExternalPlayerDB>;
}

#endif	/* EXTERNALPLAYERDB_H */

