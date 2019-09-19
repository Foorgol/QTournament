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

#ifndef KO_CONFIG_H
#define	KO_CONFIG_H

#include <QString>

#include "TournamentDataDefs.h"
#include "GroupDef.h"

namespace QTournament
{

  class KO_Config
  {
  public:
    KO_Config(KO_Start _startLevel, bool _secondSurvives, GroupDefList grps = GroupDefList());
    explicit KO_Config(QString iniString);
    explicit KO_Config(const KO_Config& orig);
    
    bool isValid(int opponentCount = -1) const;
    int getNumMatches() const;
    int getNumGroupMatches() const;
    QString toString() const;
    KO_Start getStartLevel() const;
    bool getSecondSurvives() const;
    int getNumGroupDefs() const;
    GroupDef getGroupDef(int i) const;
    int getNumReqGroups() const;
    GroupDefList getGroupDefList() const;
    int getNumGroups() const;
    int getNumRounds() const;
    
    void setStartLevel(KO_Start newLvl);
    void setSecondSurvives(bool newSurvive);
    
  private:
    KO_Start startLvl;
    bool secondSurvives;
    GroupDefList grpDefs;
  } ;
}

#endif	/* KO_CONFIG_H */

