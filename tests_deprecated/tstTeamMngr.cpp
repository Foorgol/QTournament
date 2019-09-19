/* 
 * File:   tstGenericDBObject.cpp
 * Author: volker
 * 
 * Created on March 2, 2014, 3:46 PM
 */

#include <stdexcept>

#include "tstTeamMngr.h"
#include "BasicTestClass.h"
#include "GenericDatabaseObject.h"
#include "TournamentDB.h"
#include "TournamentDataDefs.h"
#include "TeamMngr.h"
#include "TournamentErrorCodes.h"
#include "Tournament.h"

using namespace QTournament;
using namespace dbOverlay;

//----------------------------------------------------------------------------

void tstTeamMngr::testCreateNewTeam()
{
  printStartMsg("tstTeamMngr::testCreateNewTeam");
  
  TournamentDB* db = getScenario01(true);
  TeamMngr tmngr(db);
  
  // try empty or invalid name
  CPPUNIT_ASSERT(tmngr.createNewTeam("") == InvalidName);
  CPPUNIT_ASSERT(tmngr.createNewTeam(QString::null) == InvalidName);
  CPPUNIT_ASSERT((*db)[TabTeam].length() == 0);
  
  // actually create a valid team
  CPPUNIT_ASSERT(tmngr.createNewTeam("t1") == OK);
  CPPUNIT_ASSERT((*db)[TabTeam].length() == 1);
  TabRow r = (*db)[TabTeam][1];
  CPPUNIT_ASSERT(r[GenericNameFieldName].toString() == "t1");
  
  // name collision
  CPPUNIT_ASSERT(tmngr.createNewTeam("t1") == NameExists);
  CPPUNIT_ASSERT((*db)[TabTeam].length() == 1);
  
  delete db;
  printEndMsg();
}

//----------------------------------------------------------------------------

void tstTeamMngr::testCreateNewTeam2()
{
  printStartMsg("tstTeamMngr::testCreateNewTeam2");
  
  // !! start a tournament that doesn't use teams !!
  TournamentDB* db = getScenario01(false);
  TeamMngr tmngr(db);
  
  // try empty or invalid name
  CPPUNIT_ASSERT(tmngr.createNewTeam("") == NotUsingTeams);
  CPPUNIT_ASSERT(tmngr.createNewTeam(QString::null) == NotUsingTeams);
  CPPUNIT_ASSERT((*db)[TabTeam].length() == 0);
  
  // actually create a valid team
  CPPUNIT_ASSERT(tmngr.createNewTeam("t1") == NotUsingTeams);
  CPPUNIT_ASSERT((*db)[TabTeam].length() == 0);
  
  delete db;
  printEndMsg();
}

//----------------------------------------------------------------------------

void tstTeamMngr::testHasTeam()
{
  printStartMsg("tstTeamMngr::testHasTeam");
  
  TournamentDB* db = getScenario01(true);
  TeamMngr tmngr(db);
  
  // try queries on empty table
  CPPUNIT_ASSERT(tmngr.hasTeam("") == false);
  CPPUNIT_ASSERT(tmngr.hasTeam(QString::null) == false);
  CPPUNIT_ASSERT(tmngr.hasTeam("abc") == false);
  
  // actually create a valid team
  CPPUNIT_ASSERT(tmngr.createNewTeam("t1") == OK);
  
  // try queries on filled table
  CPPUNIT_ASSERT(tmngr.hasTeam("") == false);
  CPPUNIT_ASSERT(tmngr.hasTeam(QString::null) == false);
  CPPUNIT_ASSERT(tmngr.hasTeam("abc") == false);
  CPPUNIT_ASSERT(tmngr.hasTeam("t1") == true);
  
  delete db;
  printEndMsg();
}

//----------------------------------------------------------------------------

void tstTeamMngr::testGetTeam()
{
  printStartMsg("tstTeamMngr::testGetTeam");
  
  TournamentDB* db = getScenario01(true);
  TeamMngr tmngr(db);
  
  // actually create a valid team
  CPPUNIT_ASSERT(tmngr.createNewTeam("t1") == OK);
  CPPUNIT_ASSERT(tmngr.createNewTeam("t2") == OK);
  
  // try queries on filled table
  CPPUNIT_ASSERT_THROW(tmngr.getTeam(""), std::invalid_argument);
  CPPUNIT_ASSERT_THROW(tmngr.getTeam(QString::null), std::invalid_argument);
  CPPUNIT_ASSERT_THROW(tmngr.getTeam("dflsjdf"), std::invalid_argument);
  Team t = tmngr.getTeam("t2");
  CPPUNIT_ASSERT(t.getId() == 2);
  CPPUNIT_ASSERT(t.getName() == "t2");
  
  delete db;
  printEndMsg();
}

//----------------------------------------------------------------------------
    
void tstTeamMngr::testGetAllTeams()
{
  printStartMsg("tstTeamMngr::testGetAllTeams");
  
  TournamentDB* db = getScenario01(true);
  TeamMngr tmngr(db);
  
  // run on empty table
  QList<Team> result = tmngr.getAllTeams();
  CPPUNIT_ASSERT(result.length() == 0);
  
  // actually create a valid team
  CPPUNIT_ASSERT(tmngr.createNewTeam("t1") == OK);
  CPPUNIT_ASSERT(tmngr.createNewTeam("t2") == OK);
  
  // run on filled table
  result = tmngr.getAllTeams();
  CPPUNIT_ASSERT(result.length() == 2);
  Team t = result.at(0);
  CPPUNIT_ASSERT(t.getId() == 1);
  CPPUNIT_ASSERT(t.getName() == "t1");
  t = result.at(1);
  CPPUNIT_ASSERT(t.getId() == 2);
  CPPUNIT_ASSERT(t.getName() == "t2");
  
  delete db;
  printEndMsg();
}

//----------------------------------------------------------------------------

void tstTeamMngr::testChangeTeamAssignment()
{
  printStartMsg("tstTeamMngr::testChangeTeamAssignment");
  
  TournamentDB* db = getScenario02(true);
  Tournament t(getSqliteFileName());
  
  Player m1 = Tournament::getPlayerMngr()->getPlayer("f", "l1");
  Team t1 = Tournament::getTeamMngr()->getTeam("t1");
  Tournament::getTeamMngr()->createNewTeam("t2");
  Team t2 = Tournament::getTeamMngr()->getTeam("t2");
  
  // make sure m1 is assigned to t1
  CPPUNIT_ASSERT(m1.getTeam() == t1);
  
  // re-assign the team
  CPPUNIT_ASSERT(Tournament::getTeamMngr()->changeTeamAssigment(m1, t2) == OK);
  CPPUNIT_ASSERT(m1.getTeam() != t1);
  CPPUNIT_ASSERT(m1.getTeam() == t2);
  
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
    
