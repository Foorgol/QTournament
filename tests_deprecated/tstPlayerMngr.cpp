/* 
 * File:   tstGenericDBObject.cpp
 * Author: volker
 * 
 * Created on March 2, 2014, 3:46 PM
 */

#include <stdexcept>

#include "tstPlayerMngr.h"
#include "BasicTestClass.h"
#include "GenericDatabaseObject.h"
#include "TournamentDB.h"
#include "TournamentDataDefs.h"
#include "PlayerMngr.h"
#include "CatMngr.h"
#include "TeamMngr.h"
#include "TournamentErrorCodes.h"
#include "DbTab.h"
#include "Tournament.h"

using namespace QTournament;
using namespace dbOverlay;

//----------------------------------------------------------------------------

void tstPlayerMngr::testCreateNewPlayer()
{
  printStartMsg("tstPlayerMngr::testCreateNewPlayer");
  
  TournamentDB* db = getScenario01(true);
  Tournament t(getSqliteFileName());
  PlayerMngr* pmngr = Tournament::getPlayerMngr();
  
  // try empty or invalid name
  CPPUNIT_ASSERT(pmngr->createNewPlayer("", "", M, "xxx") == InvalidName);
  CPPUNIT_ASSERT(pmngr->createNewPlayer("", "abcd", M, "xxx") == InvalidName);
  CPPUNIT_ASSERT(pmngr->createNewPlayer("abcd", "", M, "xxx") == InvalidName);
  CPPUNIT_ASSERT(pmngr->createNewPlayer(QString::null, QString::null, M, "xxx") == InvalidName);
  CPPUNIT_ASSERT(pmngr->createNewPlayer(QString::null, "abcd", M, "xxx") == InvalidName);
  CPPUNIT_ASSERT(pmngr->createNewPlayer("abcd", QString::null, M, "xxx") == InvalidName);
  
  // try invalid sex
  CPPUNIT_ASSERT(pmngr->createNewPlayer("abc", "def", Sex::DontCare, "xxx") == InvalidSex);
  
  // try invalid team reference
  CPPUNIT_ASSERT(pmngr->createNewPlayer("abc", "def", M, "xxx") == InvalidTeam);
  CPPUNIT_ASSERT(pmngr->createNewPlayer("abc", "def", M, "") == InvalidTeam);
  CPPUNIT_ASSERT(pmngr->createNewPlayer("abc", "def", M, QString::null) == InvalidTeam);
  
  // make sure nothing has been inserted so far
  CPPUNIT_ASSERT((*db)[TabPlayer].length() == 0);
  
  // insert a team
  TeamMngr* tmngr = Tournament::getTeamMngr();
  CPPUNIT_ASSERT(tmngr->createNewTeam("t1") == OK);

  // insert a valid player
  CPPUNIT_ASSERT(pmngr->createNewPlayer("abc", "def", M, "t1") == OK);
  CPPUNIT_ASSERT((*db)[TabPlayer].length() == 1);
  CPPUNIT_ASSERT((*db)[TabPlayer][1][PL_Fname].toString() == "abc");
  CPPUNIT_ASSERT((*db)[TabPlayer][1][PL_Lname].toString() == "def");
  CPPUNIT_ASSERT((*db)[TabPlayer][1][PL_Sex].toInt() == 0);
  CPPUNIT_ASSERT((*db)[TabPlayer][1][PL_TeamRef].toInt() == 1);
  CPPUNIT_ASSERT((*db)[TabPlayer][1][GenericStateFieldName].toInt() == static_cast<int>(STAT_PL_Idle));
  
  // try to insert the same player again
  CPPUNIT_ASSERT(pmngr->createNewPlayer("abc", "def", M, "t1") == NameExists);
  CPPUNIT_ASSERT((*db)[TabPlayer].length() == 1);
  
  // Fake the database to become a tournament without teams
  TabRow r = (*db)[TabCfg].getSingleRowByColumnValue("K", CfgKey_UseTeams);
  r.update("V", 0);
  
  // insert valid players without valid team ref
  CPPUNIT_ASSERT(pmngr->createNewPlayer("f1", "def", M, "") == OK);
  CPPUNIT_ASSERT(pmngr->createNewPlayer("f2", "def", M, "sdklfjlsdf") == OK);
  CPPUNIT_ASSERT(pmngr->createNewPlayer("f3", "def", M, QString::null) == OK);
  CPPUNIT_ASSERT((*db)[TabPlayer].length() == 4);
  CPPUNIT_ASSERT((*db)[TabPlayer][2][PL_TeamRef].isNull());
  CPPUNIT_ASSERT((*db)[TabPlayer][3][PL_TeamRef].isNull());
  CPPUNIT_ASSERT((*db)[TabPlayer][4][PL_TeamRef].isNull());
  
  delete db;
  printEndMsg();
}

//----------------------------------------------------------------------------

void tstPlayerMngr::testHasPlayer()
{
  printStartMsg("tstPlayerMngr::testHasPlayer");
  
  TournamentDB* db = getScenario01(true);
  Tournament t(getSqliteFileName());
  PlayerMngr* pmngr = Tournament::getPlayerMngr();
  
  // try queries on empty table
  CPPUNIT_ASSERT(pmngr->hasPlayer("", "") == false);
  CPPUNIT_ASSERT(pmngr->hasPlayer(QString::null, QString::null) == false);
  CPPUNIT_ASSERT(pmngr->hasPlayer(QString::null, "") == false);
  CPPUNIT_ASSERT(pmngr->hasPlayer("", QString::null) == false);
  CPPUNIT_ASSERT(pmngr->hasPlayer("x", "") == false);
  CPPUNIT_ASSERT(pmngr->hasPlayer("", "y") == false);
  CPPUNIT_ASSERT(pmngr->hasPlayer("x", "y") == false);
  
  // actually create a valid player
  TeamMngr* tmngr = Tournament::getTeamMngr();
  CPPUNIT_ASSERT(tmngr->createNewTeam("t1") == OK);
  CPPUNIT_ASSERT(pmngr->createNewPlayer("abc", "def", M, "t1") == OK);
  
  // try queries on filled table
  CPPUNIT_ASSERT(pmngr->hasPlayer("", "") == false);
  CPPUNIT_ASSERT(pmngr->hasPlayer(QString::null, QString::null) == false);
  CPPUNIT_ASSERT(pmngr->hasPlayer(QString::null, "") == false);
  CPPUNIT_ASSERT(pmngr->hasPlayer("", QString::null) == false);
  CPPUNIT_ASSERT(pmngr->hasPlayer("x", "") == false);
  CPPUNIT_ASSERT(pmngr->hasPlayer("", "y") == false);
  CPPUNIT_ASSERT(pmngr->hasPlayer("x", "y") == false);
  CPPUNIT_ASSERT(pmngr->hasPlayer("abc", "def") == true);
  
  delete db;
  printEndMsg();
}

//----------------------------------------------------------------------------

void tstPlayerMngr::testGetPlayer()
{
  printStartMsg("tstPlayerMngr::testGetPlayer");
  
  TournamentDB* db = getScenario01(true);
  Tournament t(getSqliteFileName());
  PlayerMngr* pmngr = Tournament::getPlayerMngr();
  
  // actually create valid players
  TeamMngr* tmngr = Tournament::getTeamMngr();
  CPPUNIT_ASSERT(tmngr->createNewTeam("t1") == OK);
  CPPUNIT_ASSERT(pmngr->createNewPlayer("f1", "l1", M, "t1") == OK);
  CPPUNIT_ASSERT(pmngr->createNewPlayer("f2", "l2", M, "t1") == OK);
  
  // try queries on filled table
  CPPUNIT_ASSERT_THROW(pmngr->getPlayer("", ""), std::invalid_argument);
  CPPUNIT_ASSERT_THROW(pmngr->getPlayer(QString::null, ""), std::invalid_argument);
  CPPUNIT_ASSERT_THROW(pmngr->getPlayer("", QString::null), std::invalid_argument);
  CPPUNIT_ASSERT_THROW(pmngr->getPlayer(QString::null, QString::null), std::invalid_argument);
  CPPUNIT_ASSERT_THROW(pmngr->getPlayer("abc", ""), std::invalid_argument);
  CPPUNIT_ASSERT_THROW(pmngr->getPlayer("", "def"), std::invalid_argument);
  CPPUNIT_ASSERT_THROW(pmngr->getPlayer("abc", "def"), std::invalid_argument);
  Player p = pmngr->getPlayer("f2", "l2");
  CPPUNIT_ASSERT(p.getId() == 2);
  CPPUNIT_ASSERT(p.getDisplayName() == "l2, f2");
  
  delete db;
  printEndMsg();
}

//----------------------------------------------------------------------------
    
void tstPlayerMngr::testGetAllPlayers()
{
  printStartMsg("tstPlayerMngr::testGetAllPlayers");
  
  TournamentDB* db = getScenario01(true);
  Tournament t(getSqliteFileName());
  PlayerMngr* pmngr = Tournament::getPlayerMngr();
  
  // run on empty table
  QList<Player> result = pmngr->getAllPlayers();
  CPPUNIT_ASSERT(result.length() == 0);
  
  // actually create valid players
  TeamMngr* tmngr = Tournament::getTeamMngr();
  CPPUNIT_ASSERT(tmngr->createNewTeam("t1") == OK);
  CPPUNIT_ASSERT(pmngr->createNewPlayer("f1", "l1", M, "t1") == OK);
  CPPUNIT_ASSERT(pmngr->createNewPlayer("f2", "l2", M, "t1") == OK);
  
  // run on filled table
  result = pmngr->getAllPlayers();
  CPPUNIT_ASSERT(result.length() == 2);
  Player p = result.at(0);
  CPPUNIT_ASSERT(p.getId() == 1);
  CPPUNIT_ASSERT(p.getDisplayName() == "l1, f1");
  p = result.at(1);
  CPPUNIT_ASSERT(p.getId() == 2);
  CPPUNIT_ASSERT(p.getDisplayName() == "l2, f2");
  
  delete db;
  printEndMsg();
}

//----------------------------------------------------------------------------

void tstPlayerMngr::testRenamePlayer()
{
  printStartMsg("tstPlayerMngr::testRenamePlayer");
  
  TournamentDB* db = getScenario01(true);
  Tournament t(getSqliteFileName());
  PlayerMngr* pmngr = Tournament::getPlayerMngr();
  
  // actually create valid players
  TeamMngr* tmngr = Tournament::getTeamMngr();
  CPPUNIT_ASSERT(tmngr->createNewTeam("t1") == OK);
  CPPUNIT_ASSERT(pmngr->createNewPlayer("f1", "l1", M, "t1") == OK);
  CPPUNIT_ASSERT(pmngr->createNewPlayer("f2", "l2", M, "t1") == OK);
  
  // retrieve player object
  Player p = pmngr->getPlayer("f2", "l2");
  CPPUNIT_ASSERT(p.getId() == 2);
  CPPUNIT_ASSERT(p.getDisplayName() == "l2, f2");
  
  // rename to invalid name
  CPPUNIT_ASSERT(pmngr->renamePlayer(p, "", "") == InvalidName);
  CPPUNIT_ASSERT(pmngr->renamePlayer(p, QString::null, "") == InvalidName);
  CPPUNIT_ASSERT(pmngr->renamePlayer(p, "", QString::null) == InvalidName);
  CPPUNIT_ASSERT(pmngr->renamePlayer(p, QString::null, "") == InvalidName);
  CPPUNIT_ASSERT(p.getDisplayName() == "l2, f2");
  
  // rename to existing name
  CPPUNIT_ASSERT(pmngr->renamePlayer(p, "f1", "l1") == NameExists);
  CPPUNIT_ASSERT(p.getDisplayName() == "l2, f2");
  
  // partial rename
  CPPUNIT_ASSERT(pmngr->renamePlayer(p, "", "a") == OK);
  CPPUNIT_ASSERT(p.getDisplayName() == "a, f2");
  CPPUNIT_ASSERT(pmngr->renamePlayer(p, "b", "") == OK);
  CPPUNIT_ASSERT(p.getDisplayName() == "a, b");
  
  // full rename
  CPPUNIT_ASSERT(pmngr->renamePlayer(p, "abc", "def") == OK);
  CPPUNIT_ASSERT(p.getDisplayName() == "def, abc");

  delete db;
  printEndMsg();
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
    
