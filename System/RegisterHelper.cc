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
    for(auto theBoard : *fDetectorContainer)
    {
        std::cout<< __PRETTY_FUNCTION__ << " [" << __LINE__ << "]" << std::endl;
        theBoard->takeSnapshot();
        for(auto theOpticalGroup : *theBoard)
        {
            std::cout<< __PRETTY_FUNCTION__ << " [" << __LINE__ << "]" << std::endl;
            theOpticalGroup->takeSnapshot();
            for(auto theHybrid : *theOpticalGroup)
            {
                std::cout<< __PRETTY_FUNCTION__ << " [" << __LINE__ << "]" << std::endl;
                theHybrid->takeSnapshot();
                for(auto theChip : *theHybrid)
                {
                    std::cout<< __PRETTY_FUNCTION__ << " [" << __LINE__ << "]" << std::endl;
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
        std::cout<< __PRETTY_FUNCTION__ << " [" << __LINE__ << "]" << std::endl;
        theBoard->clearSnapshot();
        for(auto theOpticalGroup : *theBoard)
        {
            std::cout<< __PRETTY_FUNCTION__ << " [" << __LINE__ << "]" << std::endl;
            theOpticalGroup->clearSnapshot();
            for(auto theHybrid : *theOpticalGroup)
            {
                std::cout<< __PRETTY_FUNCTION__ << " [" << __LINE__ << "]" << std::endl;
                theHybrid->clearSnapshot();
                for(auto theChip : *theHybrid)
                {
                    std::cout<< __PRETTY_FUNCTION__ << " [" << __LINE__ << "]" << std::endl;
                    theChip->clearSnapshot();
                }
            }
        }
    }
}

void RegisterHelper::restoreSnapshot()
{

}

void RegisterHelper::freeFrontEndRegister(FrontEndType theFrontEndType, std::string registerName)
{
    for(auto theBoard : *fDetectorContainer)
    {
        for(auto theOpticalGroup : *theBoard)
        {
            if(theOpticalGroup->flpGBT !=nullptr)
            {
                if(theOpticalGroup->flpGBT->getFrontEndType() == theFrontEndType)
                {
                    theOpticalGroup->flpGBT->addFreeRegister(registerName);
                }
            }
            for(auto theHybrid : *theOpticalGroup)
            {
                if(fCicInterface != nullptr) // easy check if it is IT or OT
                {
                    auto theOuterTrackerHybrid = static_cast<OuterTrackerHybrid*>(theHybrid);
                    if(theOuterTrackerHybrid->fCic != nullptr)
                    {
                        if(theOuterTrackerHybrid->fCic->getFrontEndType() == theFrontEndType)
                        {
                            theOuterTrackerHybrid->fCic->addFreeRegister(registerName);
                        }
                    }
                }
                for(auto theChip : *theHybrid)
                {
                    if(theChip->getFrontEndType() == theFrontEndType)
                    {
                        theChip->addFreeRegister(registerName);
                    }
                }
            }
        }
    }
}

void RegisterHelper::freeBoardRegister(std::string registerName)
{

}


void RegisterHelper::resetTouchableRegister()
{

}
