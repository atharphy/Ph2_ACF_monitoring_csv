/*!
  \file                  RD53CalibBase.cc
  \brief                 Implementaion of CalibBase
  \author                Mauro DINARDO
  \version               1.0
  \date                  28/06/18
  Support:               email to mauro.dinardo@cern.ch
*/

#include "RD53CalibBase.h"

using namespace Ph2_HwDescription;
using namespace Ph2_HwInterface;

void CalibBase::chipErrorReport() const
{
    if(showErrorReport == true)
        for(const auto cBoard: *fDetectorContainer)
            for(const auto cOpticalGroup: *cBoard)
                for(const auto cHybrid: *cOpticalGroup)
                    for(const auto cChip: *cHybrid)
                    {
                        LOG(INFO) << GREEN << "Readout chip error report for [board/opticalGroup/hybrid/chip = " << BOLDYELLOW << cBoard->getId() << "/" << cOpticalGroup->getId() << "/"
                                  << cHybrid->getId() << "/" << +cChip->getId() << RESET << GREEN << "]" << RESET;
                        static_cast<RD53Interface*>(this->fReadoutChipInterface)->ChipErrorReport(cChip);
                    }
}

void CalibBase::copyMaskFromDefault(const std::string& which) const
{
    for(const auto cBoard: *fDetectorContainer)
        for(const auto cOpticalGroup: *cBoard)
            for(const auto cHybrid: *cOpticalGroup)
                for(const auto cChip: *cHybrid) static_cast<RD53*>(cChip)->copyMaskFromDefault(which);
}

void CalibBase::saveChipRegisters(int currentRun, bool doUpdateChip)
{
    const std::string fileReg("Run" + RD53Shared::fromInt2Str(currentRun) + "_");

    for(const auto cBoard: *fDetectorContainer)
        for(const auto cOpticalGroup: *cBoard)
        {
            for(const auto cHybrid: *cOpticalGroup)
                for(const auto cChip: *cHybrid)
                {
                    if(doUpdateChip == true) static_cast<RD53*>(cChip)->saveRegMap();
                    static_cast<RD53*>(cChip)->saveRegMap(fileReg);
                    std::string command("mv " + cChip->getFileName(fileReg) + " " + this->fDirectoryName);
                    system(command.c_str());
                    LOG(INFO) << BOLDBLUE << "\t--> Current calibration saved the configuration file for [board/opticalGroup/hybrid/chip = " << BOLDYELLOW << cBoard->getId() << "/"
                              << cOpticalGroup->getId() << "/" << cHybrid->getId() << "/" << +cChip->getId() << RESET << BOLDBLUE << "] " << BOLDYELLOW << cChip->getFileName(fileReg) << RESET;
                }

            if(cOpticalGroup->flpGBT != nullptr)
            {
                if(doUpdateChip == true) cOpticalGroup->flpGBT->saveRegMap();
                cOpticalGroup->flpGBT->saveRegMap(fileReg);
                std::string command("mv " + cOpticalGroup->flpGBT->getFileName(fileReg) + " " + this->fDirectoryName);
                system(command.c_str());

                LOG(INFO) << BOLDBLUE << "\t--> Current calibration saved the LpGBT configuration file for [board/opticalGroup = " << BOLDYELLOW << cBoard->getId() << "/" << cOpticalGroup->getId()
                          << RESET << BOLDBLUE << "] " << BOLDYELLOW << cOpticalGroup->flpGBT->getFileName(fileReg) << RESET;
            }
        }
}

void CalibBase::downloadNewDACvalues(DetectorDataContainer& DACcontainer, const std::vector<const char*>& regNames, bool checkAgainst, int value)
{
    std::vector<uint16_t> chipCommandList;
    std::vector<uint32_t> hybridCommandList;

    for(const auto cBoard: *fDetectorContainer)
        for(const auto cOpticalGroup: *cBoard)
        {
            hybridCommandList.clear();

            for(const auto cHybrid: *cOpticalGroup)
            {
                chipCommandList.clear();
                int hybridId = cHybrid->getId();

                for(const auto cChip: *cHybrid)
                    for(const auto& regName: regNames)
                    {
                        if(((checkAgainst == true) &&
                            (DACcontainer.getObject(cBoard->getId())->getObject(cOpticalGroup->getId())->getObject(cHybrid->getId())->getObject(cChip->getId())->getSummary<uint16_t>() != value)) ||
                           (checkAgainst == false))
                        {
                            static_cast<RD53Interface*>(this->fReadoutChipInterface)
                                ->PackWriteCommand(cChip,
                                                   regName,
                                                   DACcontainer.getObject(cBoard->getId())->getObject(cOpticalGroup->getId())->getObject(cHybrid->getId())->getObject(cChip->getId())->getSummary<uint16_t>(),
                                                   chipCommandList,
                                                   true);

                            LOG(INFO) << BOLDMAGENTA << ">>> " << (checkAgainst == true ? "Best " : "") << BOLDYELLOW << regName << BOLDMAGENTA
                                      << " value for [board/opticalGroup/hybrid/chip = " << BOLDYELLOW << cBoard->getId() << "/" << cOpticalGroup->getId() << "/" << cHybrid->getId() << "/"
                                      << +cChip->getId() << RESET << BOLDMAGENTA << "] = " << RESET << BOLDYELLOW
                                      << DACcontainer.getObject(cBoard->getId())->getObject(cOpticalGroup->getId())->getObject(cHybrid->getId())->getObject(cChip->getId())->getSummary<uint16_t>() << BOLDMAGENTA
                                      << " <<<" << RESET;
                        }
                        else
                        {
                            LOG(WARNING) << BOLDRED << ">>> Best " << BOLDYELLOW << regName << BOLDRED << " value for [board/opticalGroup/hybrid/chip = " << BOLDYELLOW << cBoard->getId() << "/"
                                         << cOpticalGroup->getId() << "/" << cHybrid->getId() << "/" << +cChip->getId() << BOLDRED << "] was not found <<<" << RESET;
                            return;
                        }
                    }

                static_cast<RD53Interface*>(this->fReadoutChipInterface)->PackHybridCommands(cBoard, chipCommandList, hybridId, hybridCommandList);
            }

            static_cast<RD53Interface*>(this->fReadoutChipInterface)->SendHybridCommands(cBoard, hybridCommandList);
        }
}

void CalibBase::saveSCurveOrGaindValues(const std::vector<DetectorDataContainer*>& detectorContainerVector,
                                        int                                        theCurrentRun,
                                        const std::vector<uint16_t>&               dacList,
                                        size_t                                     offset,
                                        size_t                                     nEvents,
                                        const std::string&                         name)
{
    for(const auto cBoard: *fDetectorContainer)
        for(const auto cOpticalGroup: *cBoard)
            for(const auto cHybrid: *cOpticalGroup)
                for(const auto cChip: *cHybrid)
                {
                    std::stringstream myString;
                    myString.clear();
                    myString.str("");
                    myString << this->fDirectoryName + "/Run" + RD53Shared::fromInt2Str(theCurrentRun) + "_" << name << "_"
                             << "B" << std::setfill('0') << std::setw(2) << +cBoard->getId() << "_"
                             << "O" << std::setfill('0') << std::setw(2) << +cOpticalGroup->getId() << "_"
                             << "M" << std::setfill('0') << std::setw(2) << +cHybrid->getId() << "_"
                             << "C" << std::setfill('0') << std::setw(2) << +cChip->getId() << ".dat";
                    std::ofstream fileOutID(myString.str(), std::ios::out);
                    for(auto i = 0u; i < dacList.size(); i++)
                    {
                        fileOutID << "Iteration " << i << " --- reg = " << dacList[i] - offset << std::endl;
                        for(auto row = 0u; row < RD53Shared::firstChip->getNRows(); row++)
                            for(auto col = 0u; col < RD53Shared::firstChip->getNCols(); col++)
                                if(static_cast<RD53*>(cChip)->getChipOriginalMask()->isChannelEnabled(row, col) && this->getChannelGroupHandlerContainer()
                                                                                                                       ->getObject(cBoard->getId())
                                                                                                                       ->getObject(cOpticalGroup->getId())
                                                                                                                       ->getObject(cHybrid->getId())
                                                                                                                       ->getObject(cChip->getId())
                                                                                                                       ->getSummary<std::shared_ptr<ChannelGroupHandler>>()
                                                                                                                       ->allChannelGroup()
                                                                                                                       ->isChannelEnabled(row, col))
                                    fileOutID << "r " << row << " c " << col << " h "
                                              << detectorContainerVector[i]
                                                         ->getObject(cBoard->getId())
                                                         ->getObject(cOpticalGroup->getId())
                                                         ->getObject(cHybrid->getId())
                                                         ->getObject(cChip->getId())
                                                         ->getChannel<OccupancyAndPh>(row, col)
                                                         .fOccupancy *
                                                     nEvents
                                              << " a "
                                              << detectorContainerVector[i]
                                                     ->getObject(cBoard->getId())
                                                     ->getObject(cOpticalGroup->getId())
                                                     ->getObject(cHybrid->getId())
                                                     ->getObject(cChip->getId())
                                                     ->getChannel<OccupancyAndPh>(row, col)
                                                     .fPh
                                              << std::endl;
                    }
                    fileOutID.close();
                }
}

uint8_t CalibBase::assignGroupType(RD53Shared::INJtype injType) const
{
    auto groupType = (injType == RD53Shared::INJtype::None) ? RD53GroupType::AllPixels : RD53GroupType::Groups;

    if(injType == RD53Shared::INJtype::XtalkCoupled)
        groupType = RD53GroupType::XtalkCoupled;
    else if(injType == RD53Shared::INJtype::XtalkDeCoupled)
        groupType = RD53GroupType::XtalkDeCoupled;
    else if(injType == RD53Shared::INJtype::Custom)
        groupType = RD53GroupType::Custom;

    return groupType;
}
