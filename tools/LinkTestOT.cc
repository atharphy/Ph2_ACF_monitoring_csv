#include "LinkTestOT.h"

#include "../Utils/CBCChannelGroupHandler.h"
#include "../Utils/ContainerFactory.h"

using namespace Ph2_HwDescription;
using namespace Ph2_HwInterface;
using namespace Ph2_System;

LinkTestOT::LinkTestOT() : LinkAlignmentOT() {}
LinkTestOT::~LinkTestOT() {}

// test L1A + stub components of link data
void LinkTestOT::TestL1ALines(bool pAlign)
{
    for(auto cBoard: *fDetectorContainer) { TestL1ALine(cBoard, pAlign); }
}
bool LinkTestOT::TestL1ALine(BeBoard* pBoard, bool pAlign, size_t pAttempts)
{
    std::string cAllZeros          = "00000000000000000000000000000000";
    uint32_t    cTriggerRateToTest = 1000; // kHz
    if(pAlign)
    {
        LinkAlignmentOT::Inherit(this);
        LinkAlignmentOT::Initialise();

        if(!PhaseAlignBEdata(pBoard)) return false;
        if(!WordAlignBEdata(pBoard)) return false;

        LinkAlignmentOT::Reset();
    }

    // now ... configure CIC to produce idle pattern
    // on L1 line
    fBeBoardInterface->setBoard(pBoard->getId());
    auto cInterface = static_cast<D19cFWInterface*>(fBeBoardInterface->getFirmwareInterface());
    // make sure that we are in sparsified mode
    bool cSparsified = pBoard->getSparsification();
    pBoard->setSparsification(true);
    // and that the trigger rate is low so I only get one L1 packet
    uint16_t cTriggerRate = fBeBoardInterface->ReadBoardReg(pBoard, "fc7_daq_cnfg.fast_command_block.user_trigger_frequency");
    LOG(INFO) << BOLDBLUE << "Trigger rate will be set to " << +cTriggerRateToTest << " kHz for L1A data test" << RESET;
    std::vector<std::pair<std::string, uint32_t>> cRegVec;
    cRegVec.push_back({"fc7_daq_cnfg.fast_command_block.user_trigger_frequency", cTriggerRateToTest});
    fBeBoardInterface->WriteBoardMultReg(pBoard, cRegVec);
    // capture data
    for(auto cOpticalGroup: *pBoard)
    {
        // also disable all FEs to make sure that only the header appears
        std::vector<uint8_t> cFeEnableRegs(0);
        for(auto cHybrid: *cOpticalGroup)
        {
            auto& cCic = static_cast<OuterTrackerHybrid*>(cHybrid)->fCic;
            // make sure we are transmitting sparsified data
            fCicInterface->SetSparsification(cCic, true);
            // disable alignment output
            cFeEnableRegs.push_back(fCicInterface->ReadChipReg(cCic, "FE_ENABLE"));
            fCicInterface->EnableFEs(cCic, {0, 1, 2, 3, 4, 5, 6, 7}, false);
        }

        // now get L1A debug data from each hybrid
        for(auto cHybrid: *cOpticalGroup)
        {
            fBeBoardInterface->WriteBoardReg(pBoard, "fc7_daq_cnfg.physical_interface_block.slvs_debug.hybrid_select", cHybrid->getId());
            fBeBoardInterface->WriteBoardReg(pBoard, "fc7_daq_cnfg.physical_interface_block.slvs_debug.chip_select", 0);

            // count headers
            size_t cDetectedHeaders = 0;
            size_t cGoodHeaders     = 0;
            // count L1 Ids
            size_t cDetectedL1Ids = 0;
            size_t cGoodL1Ids     = 0;
            // count bits transmitted in idle pattern
            std::vector<float>  cIdleBalance(0);
            std::vector<size_t> cIdleBits(0);
            for(size_t cAttempt = 0; cAttempt < pAttempts; cAttempt++)
            {
                auto        cBuffer     = cInterface->L1ADebug(1, false);
                std::string cIdleBuffer = cBuffer;
                // if buffer ends in 3 0s .. skip
                if(cBuffer.substr(cBuffer.length() - 5, 5) == "00000") continue;

                LOG(DEBUG) << BOLDMAGENTA << cBuffer << RESET;
                // remove CIC headers
                size_t                cPos     = 0;
                size_t                cPosCopy = std::string::npos;
                auto                  cFound   = cBuffer.find("111111111111111111111110", cPos);
                std::vector<uint16_t> cL1Ids(0);
                do
                {
                    auto cHeader       = cBuffer.substr(cFound - 8, 32);
                    auto cStatus       = cBuffer.substr(cFound - 8 + 32, 9);
                    auto cL1Id         = cBuffer.substr(cFound - 8 + 32 + 9, 9);
                    int  cHeader1Count = std::count_if(cHeader.begin(), cHeader.end(), [](char c) { return c == '1'; });
                    cGoodHeaders += (cHeader1Count == 27) ? 1 : 0;
                    cDetectedHeaders += 1;
                    std::string cCicHeader = cBuffer.substr(cFound - 8, 32) + cStatus + cL1Id;
                    LOG(DEBUG) << BOLDBLUE << "\t\t header : " << cHeader << " - status is " << cStatus << " - L1 Id is " << cL1Id << RESET;
                    if((cPosCopy = cIdleBuffer.find(cCicHeader)) != std::string::npos) { cIdleBuffer.erase(cPosCopy, cCicHeader.length()); }
                    cPos   = (size_t)cFound + 24;
                    cFound = cBuffer.find("111111111111111111111110", cPos);
                    cL1Ids.push_back(std::stoi(cL1Id, 0, 2));
                } while(cFound != std::string::npos);

                // extract idle pattern
                cPos = 0;
                LOG(DEBUG) << BOLDBLUE << cIdleBuffer << RESET;
                while((cFound = cIdleBuffer.find("100", cPos)) != std::string::npos)
                {
                    auto cEnd = cIdleBuffer.find("001", cFound);
                    if(cEnd != std::string::npos)
                    {
                        auto cStart = cFound + 2;
                        cEnd += 2;
                        cIdleBuffer.erase(cStart, cEnd - cStart);
                        // LOG (INFO) << "\t\t" << BOLDGREEN << cIdleBuffer << RESET;
                    }
                }
                float cIdle1Cnt = std::count_if(cIdleBuffer.begin(), cIdleBuffer.end(), [](char c) { return c == '1'; });
                float cIdle0Cnt = std::count_if(cIdleBuffer.begin(), cIdleBuffer.end(), [](char c) { return c == '0'; });
                float cIdleBlnc = cIdle1Cnt / cIdle0Cnt;
                cIdleBalance.push_back(cIdleBlnc);
                cIdleBits.push_back(cIdleBuffer.length());
                LOG(INFO) << BOLDBLUE << "Attempt#" << +cAttempt << " ... idle data contains " << +cIdleBits[cIdleBits.size() - 1] << " bits; balance is " << cIdleBalance[cIdleBalance.size() - 1]
                          << RESET;
                LOG(DEBUG) << BOLDYELLOW << cIdleBuffer << " [1's : " << +cIdle1Cnt << " ][0's :  " << cIdle0Cnt << " ] " << cIdleBlnc << RESET;

                if(cL1Ids.size() == 1) continue;

                auto cIter = cL1Ids.begin() + 1;
                // count good L1Ids
                do
                {
                    int cPrevious = *(cIter - 1);
                    if(cPrevious == 511) cPrevious = -1;
                    int cDiff = *(cIter)-cPrevious;
                    cGoodL1Ids += (cDiff == 1) ? 1 : 0;
                    LOG(DEBUG) << BOLDBLUE << cDiff << RESET;
                    cIter++;
                    cDetectedL1Ids++;
                } while(cIter < cL1Ids.end());
            }
            auto cStatsIdle = getStats(cIdleBalance);
            LOG(INFO) << BOLDBLUE << "In total " << cIdleBalance.size() << " idle frames tested. Average 1:0 ratio found to be " << cStatsIdle.first << " sdev is " << cStatsIdle.second << RESET;
            LOG(INFO) << BOLDBLUE << "In total " << cDetectedHeaders << " headers detected. Found " << cGoodHeaders << " with the correct number of 1s" << RESET;
            LOG(INFO) << BOLDBLUE << "In total " << cDetectedL1Ids << " L1Ids detected. Found " << cGoodL1Ids << " that increment by 1." << RESET;
        }
        size_t cIndx = 0;
        for(auto cHybrid: *cOpticalGroup)
        {
            auto& cCic = static_cast<OuterTrackerHybrid*>(cHybrid)->fCic;
            fCicInterface->SetSparsification(cCic, cSparsified);
            fCicInterface->WriteChipReg(cCic, "FE_ENABLE", cFeEnableRegs[cIndx]);
            cIndx++;
        }
    }
    // reset board registers/settings
    pBoard->setSparsification(cSparsified);
    cRegVec.clear();
    cRegVec.push_back({"fc7_daq_cnfg.fast_command_block.user_trigger_frequency", cTriggerRate});
    fBeBoardInterface->WriteBoardMultReg(pBoard, cRegVec);
    return true;
}
// State machine control functions
void LinkTestOT::Running() {}

void LinkTestOT::Stop() {}

void LinkTestOT::Pause() {}

void LinkTestOT::Resume() {}