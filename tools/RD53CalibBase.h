/*!
  \file                  RD53CalibBase.h
  \brief                 Implementaion of CalibBase
  \author                Mauro DINARDO
  \version               1.0
  \date                  28/06/18
  Support:               email to mauro.dinardo@cern.ch
*/

#ifndef RD53CalibBase_H
#define RD53CalibBase_H

#include "../HWDescription/RD53.h"
#include "../Utils/Container.h"
#include "../Utils/ContainerFactory.h"
#include "../Utils/RD53ChannelGroupHandler.h"
#include "Tool.h"

// ##################################
// # Basic class for IT calibration #
// ##################################
class CalibBase : public Tool
{
    public:
        void chipErrorReport() const;
        void saveChipRegisters(int currentRun, bool doUpdateChip);
        void downloadNewDACvalues();
        void saveSCurveORGaindValues(const std::string& name);
};

#endif
