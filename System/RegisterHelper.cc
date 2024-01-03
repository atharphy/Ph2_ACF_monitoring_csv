#include "System/RegisterHelper.h"
#include "Utils/Container.h"
#include "HWInterface/BeBoardInterface.h"
#include "HWInterface/CicInterface.h"
#include "HWInterface/lpGBTInterface.h"
#include "HWDescription/Definition.h"
#include "HWDescription/OuterTrackerHybrid.h"

#include "iostream"

using namespace Ph2_System;
using namespace Ph2_HwInterface;
using namespace Ph2_HwDescription;

RegisterHelper::RegisterHelper(DetectorContainer* theDetectorContainer,
                               BeBoardInterface* theBeBoardInterface,
                               ReadoutChipInterface* theReadoutChipInterface,
                               lpGBTInterface* thelpGBTInterface,
                               CicInterface* theCicInterface)
                               : fDetectorContainer(theDetectorContainer)
                               , fBeBoardInterface(theBeBoardInterface)
                               , fReadoutChipInterface(theReadoutChipInterface)
                               , flpGBTInterface(thelpGBTInterface)
                               , fCicInterface(theCicInterface)
{}

void RegisterHelper::takeSnapshot()
{
    LOG(INFO) << BOLDYELLOW << __PRETTY_FUNCTION__ << " taking snapshot of the current HW configuration" << RESET;
    for(auto theBoard : *fDetectorContainer)
    {
        theBoard->takeSnapshot();
        for(auto theOpticalGroup : *theBoard)
        {
            auto theLpGBT = theOpticalGroup->flpGBT;
            if(theLpGBT != nullptr)
            {
                theLpGBT->takeSnapshot();
            }
            for(auto theHybrid : *theOpticalGroup)
            {
                if(fCicInterface != nullptr) // easy check if it is IT or OT
                {
                    auto theCic = static_cast<OuterTrackerHybrid*>(theHybrid)->fCic;
                    if(theCic != nullptr)
                    {
                        theCic->takeSnapshot();
                    }
                }
                for(auto theChip : *theHybrid)
                {
                    theChip->takeSnapshot();
                }
            }
        }
    }
}

void RegisterHelper::clearSnapshot()
{
    for(auto theBoard : *fDetectorContainer)
    {
        theBoard->clearSnapshot();
        for(auto theOpticalGroup : *theBoard)
        {
            auto theLpGBT = theOpticalGroup->flpGBT;
            if(theLpGBT != nullptr)
            {
                theLpGBT->clearSnapshot();
            }
            for(auto theHybrid : *theOpticalGroup)
            {
                if(fCicInterface != nullptr) // easy check if it is IT or OT
                {
                    auto theCic = static_cast<OuterTrackerHybrid*>(theHybrid)->fCic;
                    if(theCic != nullptr)
                    {
                        theCic->clearSnapshot();
                    }
                }
                for(auto theChip : *theHybrid)
                {
                    theChip->clearSnapshot();
                }
            }
        }
    }
}

void RegisterHelper::restoreSnapshot()
{
    LOG(INFO) << BOLDYELLOW << __PRETTY_FUNCTION__ << " restoring snapshot of the HW configuration" << RESET;

    for(auto theBoard : *fDetectorContainer)
    {
        const auto modifiedBoardRegisters = theBoard->getSnapshot();
        fBeBoardInterface->WriteBoardMultReg(theBoard, modifiedBoardRegisters);
        for(auto theOpticalGroup : *theBoard)
        {
            auto theLpGBT = theOpticalGroup->flpGBT;
            if(theLpGBT != nullptr)
            {
                const auto modifiedLpGBTRegisters = theLpGBT->getSnapshot();
                flpGBTInterface->WriteChipMultReg(theLpGBT, modifiedLpGBTRegisters);
            }
            for(auto theHybrid : *theOpticalGroup)
            {
                if(fCicInterface != nullptr) // easy check if it is IT or OT
                {
                    auto theCic = static_cast<OuterTrackerHybrid*>(theHybrid)->fCic;
                    if(theCic != nullptr)
                    {
                        const auto modifiedCicRegisters = theCic->getSnapshot();
                        fCicInterface->WriteChipMultReg(theCic, modifiedCicRegisters);
                    }
                }
                for(auto theChip : *theHybrid)
                {
                    const auto modifiedChipRegisters = theChip->getSnapshot();
                    fReadoutChipInterface->WriteChipMultReg(theChip, modifiedChipRegisters);
                }
            }
        }
    }

    clearSnapshot();
    resetFreeRegisters();
}

void RegisterHelper::freeFrontEndRegister(const FrontEndType theFrontEndType, std::string registerName)
{
    LOG(INFO) << BOLDYELLOW << __PRETTY_FUNCTION__ << " Freeing registers matching pattern " << registerName << " for frontend type " << FrontEndDescription::getFrontEndName(theFrontEndType) << RESET;

    std::regex registerPattern(registerName);
    for(auto theBoard : *fDetectorContainer)
    {
        for(auto theOpticalGroup : *theBoard)
        {
            if(theOpticalGroup->flpGBT !=nullptr)
            {
                if(theOpticalGroup->flpGBT->getFrontEndType() == theFrontEndType)
                {
                    theOpticalGroup->flpGBT->addFreeRegister(registerPattern);
                }
            }
            for(auto theHybrid : *theOpticalGroup)
            {
                if(fCicInterface != nullptr) // easy check if it is IT or OT
                {
                    auto theCic = static_cast<OuterTrackerHybrid*>(theHybrid)->fCic;
                    if(theCic != nullptr)
                    {
                        if(theCic->getFrontEndType() == theFrontEndType)
                        {
                            theCic->addFreeRegister(registerPattern);
                        }
                    }
                }
                for(auto theChip : *theHybrid)
                {
                    if(theChip->getFrontEndType() == theFrontEndType)
                    {
                        theChip->addFreeRegister(registerPattern);
                    }
                }
            }
        }
    }
}

void RegisterHelper::freeBoardRegister(std::string registerName)
{
    std::regex registerPattern(registerName);
    for(auto theBoard : *fDetectorContainer)
    {
        theBoard->addFreeRegister(registerPattern);
    }
}


void RegisterHelper::resetFreeRegisters()
{
    for(auto theBoard : *fDetectorContainer)
    {
        theBoard->clearFreeRegisters();
        for(auto theOpticalGroup : *theBoard)
        {
            auto theLpGBT = theOpticalGroup->flpGBT;
            if(theLpGBT != nullptr)
            {
                theLpGBT->clearFreeRegisters();
            }
            for(auto theHybrid : *theOpticalGroup)
            {
                if(fCicInterface != nullptr) // easy check if it is IT or OT
                {
                    auto theCic = static_cast<OuterTrackerHybrid*>(theHybrid)->fCic;
                    if(theCic != nullptr)
                    {
                        theCic->clearFreeRegisters();
                    }
                }
                for(auto theChip : *theHybrid)
                {
                    theChip->clearFreeRegisters();
                }
            }
        }
    }
}
