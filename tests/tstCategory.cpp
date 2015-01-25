/* 
 * File:   tstGenericDBObject.cpp
 * Author: volker
 * 
 * Created on March 2, 2014, 3:46 PM
 */

#include <stdexcept>

#include "tstCategory.h"
#include "BasicTestClass.h"
#include "GenericDatabaseObject.h"
#include "TournamentDB.h"
#include "TournamentDataDefs.h"
#include "Category.h"
#include "TournamentErrorCodes.h"
#include "Tournament.h"
#include "tstPlayerMngr.h"

using namespace QTournament;
using namespace dbOverlay;

//----------------------------------------------------------------------------

void tstCategory::testGetAddState()
{
  printStartMsg("tstCategory::testGetAddState");
  
  TournamentDB* db = getScenario02(true);
  Tournament t(getSqliteFileName());
  
  // create a team some dummy players
  Player m1 = Tournament::getPlayerMngr()->getPlayer("f", "l1");
  Player f1 = Tournament::getPlayerMngr()->getPlayer("f", "l2");
  
  // create one category of every kind
  CatMngr* cmngr = Tournament::getCatMngr();
  Category ms = cmngr->getCategory("MS");
  Category md = cmngr->getCategory("MD");
  Category ls = cmngr->getCategory("LS");
  Category ld = cmngr->getCategory("LD");
  Category mx = cmngr->getCategory("MX");
  
  // check player suitability for men's single
  CPPUNIT_ASSERT(ms.getAddState(m1) == CAN_JOIN);
  CPPUNIT_ASSERT(ms.getAddState(f1) == WRONG_SEX);
  ms.setSex(DONT_CARE);   // disable checking
  CPPUNIT_ASSERT(ms.getAddState(m1) == CAN_JOIN);
  CPPUNIT_ASSERT(ms.getAddState(f1) == CAN_JOIN);
  
  // check player suitability for men's doubles
  CPPUNIT_ASSERT(md.getAddState(m1) == CAN_JOIN);
  CPPUNIT_ASSERT(md.getAddState(f1) == WRONG_SEX);
  md.setSex(DONT_CARE);   // disable checking
  CPPUNIT_ASSERT(md.getAddState(m1) == CAN_JOIN);
  CPPUNIT_ASSERT(md.getAddState(f1) == CAN_JOIN);
  
  // check player suitability for ladies' single
  CPPUNIT_ASSERT(ls.getAddState(m1) == WRONG_SEX);
  CPPUNIT_ASSERT(ls.getAddState(f1) == CAN_JOIN);
  ls.setSex(DONT_CARE);   // disable checking
  CPPUNIT_ASSERT(ls.getAddState(m1) == CAN_JOIN);
  CPPUNIT_ASSERT(ls.getAddState(f1) == CAN_JOIN);
  
  // check player suitability for ladies' doubles
  CPPUNIT_ASSERT(ld.getAddState(m1) == WRONG_SEX);
  CPPUNIT_ASSERT(ld.getAddState(f1) == CAN_JOIN);
  ld.setSex(DONT_CARE);   // disable checking
  CPPUNIT_ASSERT(ld.getAddState(m1) == CAN_JOIN);
  CPPUNIT_ASSERT(ld.getAddState(f1) == CAN_JOIN);
  
  // check player suitability for mixed doubles
  CPPUNIT_ASSERT(mx.getAddState(m1) == CAN_JOIN);
  CPPUNIT_ASSERT(mx.getAddState(f1) == CAN_JOIN);
  mx.setSex(DONT_CARE);   // disable checking
  CPPUNIT_ASSERT(mx.getAddState(m1) == CAN_JOIN);
  CPPUNIT_ASSERT(mx.getAddState(f1) == CAN_JOIN);
  
  // TODO:
  // add test cases for when a category is closed
  // or for previously added players
  // (return values "ALREADY_MEMBER" and "CAT_CLOSED")
  
  delete db;
  printEndMsg();
}

//----------------------------------------------------------------------------

void tstCategory::testHasPlayer()
{
  printStartMsg("tstCategory::testHasPlayer");
  
  TournamentDB* db = getScenario02(true);
  Tournament t(getSqliteFileName());
  
  // create a team some dummy players
  Player m1 = Tournament::getPlayerMngr()->getPlayer("f", "l1");
  Player f1 = Tournament::getPlayerMngr()->getPlayer("f", "l2");
  Player m2 = Tournament::getPlayerMngr()->getPlayer("f", "l3");
  Player f2 = Tournament::getPlayerMngr()->getPlayer("f", "l4");
  
  // create a category
  CatMngr* cmngr = Tournament::getCatMngr();
  Category ms = cmngr->getCategory("MS");
  
  // query empty category
  CPPUNIT_ASSERT(ms.hasPlayer(m1) == false);
  
  // add a player
  CPPUNIT_ASSERT(cmngr->addPlayerToCategory(m1, ms) == OK);
  
  // query filled category
  CPPUNIT_ASSERT(ms.hasPlayer(m1) == true);
  CPPUNIT_ASSERT(ms.hasPlayer(f1) == false);
  CPPUNIT_ASSERT(ms.hasPlayer(m2) == false);
  CPPUNIT_ASSERT(ms.hasPlayer(f2) == false);
  
  delete db;
  printEndMsg();
}

//----------------------------------------------------------------------------

void tstCategory::testCanPair()
{
  printStartMsg("tstCategory::testCanPair");
  
  TournamentDB* db = getScenario03(true);
  Tournament t(getSqliteFileName());
  
  // create a team some dummy players
  Player m1 = Tournament::getPlayerMngr()->getPlayer("f", "l1");
  Player f1 = Tournament::getPlayerMngr()->getPlayer("f", "l2");
  Player m2 = Tournament::getPlayerMngr()->getPlayer("f", "l3");
  Player f2 = Tournament::getPlayerMngr()->getPlayer("f", "l4");
  Player m3 = Tournament::getPlayerMngr()->getPlayer("f", "l5");
  
  // create a category
  CatMngr* cmngr = Tournament::getCatMngr();
  Category md = cmngr->getCategory("MD");
  Category ms = cmngr->getCategory("MS");
  Category mx = cmngr->getCategory("MX");
  
  // try to pair players in a non-pairable category
  CPPUNIT_ASSERT(ms.canPairPlayers(m1, m2) == NO_CATEGORY_FOR_PAIRING);
  
  // try to pair with a player not in the category
  CPPUNIT_ASSERT(md.canPairPlayers(m1, m3) == PLAYER_NOT_IN_CATEGORY);
  CPPUNIT_ASSERT(md.canPairPlayers(m3, m1) == PLAYER_NOT_IN_CATEGORY);
  
  // try identical players
  CPPUNIT_ASSERT(md.canPairPlayers(m1, m1) == PLAYERS_IDENTICAL);
  
  // try to pair same-sex players in mixed categories
  CPPUNIT_ASSERT(mx.canPairPlayers(m1, m2) == INVALID_SEX);
  CPPUNIT_ASSERT(mx.canPairPlayers(f1, f2) == INVALID_SEX);
  
  // try to pair same-sex players in mixed categories
  // after setting it to "Don't care"
  cmngr->setSex(mx, DONT_CARE);
  CPPUNIT_ASSERT(mx.canPairPlayers(m1, m2) == OK);
  CPPUNIT_ASSERT(mx.canPairPlayers(f1, f2) == OK);
  
  // try to pair valid players
  CPPUNIT_ASSERT(md.canPairPlayers(m1, m2) == OK);
  
  // make sure existing pairs can't be paired again
  CPPUNIT_ASSERT(cmngr->pairPlayers(md, m1,m2) == OK);
  CPPUNIT_ASSERT(md.canPairPlayers(m1, m2) == PLAYER_ALREADY_PAIRED);
  CPPUNIT_ASSERT(cmngr->addPlayerToCategory(m3, md) == OK);
  CPPUNIT_ASSERT(md.canPairPlayers(m1, m3) == PLAYER_ALREADY_PAIRED);
  CPPUNIT_ASSERT(md.canPairPlayers(m3, m1) == PLAYER_ALREADY_PAIRED);
  CPPUNIT_ASSERT(md.canPairPlayers(m2, m3) == PLAYER_ALREADY_PAIRED);
  CPPUNIT_ASSERT(md.canPairPlayers(m3, m2) == PLAYER_ALREADY_PAIRED);
  
  delete db;
  printEndMsg();
}

//----------------------------------------------------------------------------

void tstCategory::testCanSplit()
{
  printStartMsg("tstCategory::testCanPair");
  
  TournamentDB* db = getScenario03(true);
  Tournament t(getSqliteFileName());
  
  // create a team some dummy players
  Player m1 = Tournament::getPlayerMngr()->getPlayer("f", "l1");
  Player f1 = Tournament::getPlayerMngr()->getPlayer("f", "l2");
  Player m2 = Tournament::getPlayerMngr()->getPlayer("f", "l3");
  Player f2 = Tournament::getPlayerMngr()->getPlayer("f", "l4");
  Player m3 = Tournament::getPlayerMngr()->getPlayer("f", "l5");
  
  // create a category
  CatMngr* cmngr = Tournament::getCatMngr();
  Category md = cmngr->getCategory("MD");
  Category ms = cmngr->getCategory("MS");
  Category mx = cmngr->getCategory("MX");
  mx.setSex(DONT_CARE);
  
  // create a valid player pair and an identical pair in another
  // category
  CPPUNIT_ASSERT(cmngr->pairPlayers(md, m1,m2) == OK);
  
  // try to split non-paired players
  CPPUNIT_ASSERT(cmngr->splitPlayers(md, m1, m3) == PLAYERS_NOT_A_PAIR);
  CPPUNIT_ASSERT(cmngr->splitPlayers(md, m3, m1) == PLAYERS_NOT_A_PAIR);
  
  // try to split identical players
  CPPUNIT_ASSERT(cmngr->splitPlayers(md, m1, m1) == PLAYERS_NOT_A_PAIR);
  
  // valid request
  CPPUNIT_ASSERT(cmngr->splitPlayers(md, m1, m2) == OK);

  // similar request, but for another category  
  CPPUNIT_ASSERT(cmngr->splitPlayers(mx, m1, m2) == PLAYERS_NOT_A_PAIR);
  
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
    
