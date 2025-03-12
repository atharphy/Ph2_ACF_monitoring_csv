#include "tools/OTSSAtoSSAecv.h"
#include "HWInterface/D19cFWInterface.h"
#include "HWInterface/SSA2Interface.h"
#include "System/RegisterHelper.h"
#include "Utils/ContainerSerialization.h"
#include "Utils/GenericDataArray.h"

using namespace Ph2_HwDescription;
using namespace Ph2_HwInterface;
using namespace Ph2_System;

std::string OTSSAtoSSAecv::fCalibrationDescription = "Scan phase and strenght on the SSA to SSA lines";

OTSSAtoSSAecv::OTSSAtoSSAecv() : OTSSAtoMPAecv() {}

OTSSAtoSSAecv::~OTSSAtoSSAecv() {}

void OTSSAtoSSAecv::Initialise(void)
{
    fRegisterHelper->takeSnapshot();
    // free the registers in case any
    fNumberOfStubBits      = findValueInSettings<double>("OTverifyCICdataWord_NumberOfTestedStubBits", 1e8);
    fNumberOfL1Bits        = findValueInSettings<double>("OTverifyCICdataWord_NumberOfTestedL1Bits", 1e6);
    fListOfSSAslvsCurrents = convertStringToFloatList(findValueInSettings<std::string>("OTSSAtoSSAecv_ListOfSSAslvsCurrents", "1, 4, 7"));

    ContainerFactory::copyAndInitHybrid<GenericDataArray<float, NUMBER_OF_CIC_PORTS, 9>>(*fDetectorContainer, fPatternMatchingEfficiencyContainer);

    fBendingToCode = {{-7, 1}, {-5, 2}, {-3, 3}, {+3, 5}, {+5, 6}, {+7, 7}};

#ifdef __USE_ROOT__
    // Calibration is not running on the SoC: plots are booked during initialization
    fDQMHistogramOTSSAtoSSAecv.book(fResultFile, *fDetectorContainer, fSettingsMap);
#endif
}

void OTSSAtoSSAecv::ConfigureCalibration() {}

void OTSSAtoSSAecv::Running()
{
    LOG(INFO) << "Starting OTSSAtoSSAecv measurement.";
    Initialise();
    runSSAtoSSAecvScan();
    LOG(INFO) << "Done with OTSSAtoSSAecv.";
    Reset();
}

void OTSSAtoSSAecv::Stop(void)
{
    LOG(INFO) << "Stopping OTSSAtoSSAecv measurement.";
#ifdef __USE_ROOT__
    // Calibration is not running on the SoC: processing the histograms
    fDQMHistogramOTSSAtoSSAecv.process();
#endif
    SaveResults();
    closeFileHandler();
    LOG(INFO) << "OTSSAtoSSAecv stopped.";
}

void OTSSAtoSSAecv::Pause() {}

void OTSSAtoSSAecv::Resume() {}

void OTSSAtoSSAecv::Reset() { fRegisterHelper->restoreSnapshot(); }

void OTSSAtoSSAecv::runSSAtoSSAecvScan()
{
    for(uint8_t slvsCurrent: fListOfSSAslvsCurrents)
    {
        LOG(INFO) << BOLDGREEN << "Scanning SSA SLVS current = " << +slvsCurrent << RESET;
        auto        SSAqueryFunction          = [](const ChipContainer* theChip) { return (static_cast<const ReadoutChip*>(theChip)->getFrontEndType() == FrontEndType::SSA2); };
        std::string theSSAqueryFunctionString = "SSAqueryFunction";
        // settings for SSAs
        fDetectorContainer->addReadoutChipQueryFunction(SSAqueryFunction, theSSAqueryFunctionString);
        for(auto theBoard: *fDetectorContainer)
        {
            for(auto theOpticalGroup: *theBoard)
            {
                for(auto theHybrid: *theOpticalGroup)
                {
                    for(auto theChip: *theHybrid)
                    {
                        static_cast<PSInterface*>(fReadoutChipInterface)
                            ->fTheSSA2Interface->WriteChipRegBits(theChip, "SLVS_pad_current_Lateral", slvsCurrent | (slvsCurrent << 3), "mask_peri_D", 0x3F);
                    }
                }
            }
        }
        fDetectorContainer->removeReadoutChipQueryFunction(theSSAqueryFunctionString);

        std::vector<uint8_t> injectedStripList = {1, 118};
        for(auto injectedStrip: injectedStripList)
        {
            fCurrentStripInjected = injectedStrip;

            LOG(INFO) << BOLDGREEN << "Scanning SSA to SSA stub phases for " << (injectedStrip == 1 ? "Left to Right" : "Right to Left") << " lateral communication" << RESET;
            for(uint8_t clockEdge = 0; clockEdge < 2; ++clockEdge)
            {
                LOG(INFO) << BOLDGREEN << "SSA sampling clockEdge = " << +clockEdge << RESET;

                resetPatternMatchingEfficiencyContainer();

                // setting phases and driver strenghts
                for(auto theBoard: *fDetectorContainer)
                {
                    for(auto theOpticalGroup: *theBoard)
                    {
                        for(auto theHybrid: *theOpticalGroup)
                        {
                            for(auto theChip: *theHybrid)
                            {
                                if(theChip->getFrontEndType() == FrontEndType::SSA2)
                                {
                                    auto theSSAInterface = static_cast<PSInterface*>(fReadoutChipInterface)->fTheSSA2Interface;
                                    theSSAInterface->WriteChipRegBits(theChip, "LateralRX_sampling", (clockEdge << 3) | (clockEdge << 7), "mask_peri_D", 0x88);
                                }
                            }
                        }
                    }
                    // auto theFWInterface = static_cast<D19cFWInterface*>(fBeBoardInterface->getFirmwareInterface(theBoard));
                    // runStubIntegrityTestPS(theBoard, theFWInterface);
                }

#ifdef __USE_ROOT__
                fDQMHistogramOTSSAtoSSAecv.fillStubPatternEfficiencyScan(fPatternMatchingEfficiencyContainer, injectedStrip, clockEdge, slvsCurrent);
#else
                if(fDQMStreamerEnabled)
                {
                    ContainerSerialization thePatternMatchingEfficiencyContainerSerialization("OTSSAtoSSAecvStubPatternMatchingEfficiency");
                    thePatternMatchingEfficiencyContainerSerialization.streamByHybridContainer(fDQMStreamer, fPatternMatchingEfficiencyContainer, injectedStrip, clockEdge, slvsCurrent);
                }
#endif
            }
        }
    }
}

void OTSSAtoSSAecv::setStubLogicParameters(ReadoutChip* theMPA)
{
    fReadoutChipInterface->WriteChipReg(theMPA, "StubWindow", 8);
    fReadoutChipInterface->WriteChipReg(theMPA, "StubMode", 0);
    fReadoutChipInterface->WriteChipReg(theMPA, "CodeDM8", 0x00);
    fReadoutChipInterface->WriteChipReg(theMPA, "CodeM76", fBendingToCode.at(-7) << 3);
    fReadoutChipInterface->WriteChipReg(theMPA, "CodeM54", fBendingToCode.at(-5) << 3);
    fReadoutChipInterface->WriteChipReg(theMPA, "CodeM32", fBendingToCode.at(-3) << 3);
    fReadoutChipInterface->WriteChipReg(theMPA, "CodeM10", 0x00);
    fReadoutChipInterface->WriteChipReg(theMPA, "CodeP12", 0x00);
    fReadoutChipInterface->WriteChipReg(theMPA, "CodeP34", fBendingToCode.at(+3) << 3);
    fReadoutChipInterface->WriteChipReg(theMPA, "CodeP56", fBendingToCode.at(+5) << 3);
    fReadoutChipInterface->WriteChipReg(theMPA, "CodeP78", fBendingToCode.at(+7) << 3);
}

std::vector<Cluster> OTSSAtoSSAecv::produceStripClusterList()
{
    std::vector<Cluster> listOfInjectedStrips;

    listOfInjectedStrips.push_back(Cluster(0, fCurrentStripInjected, 1));

    return listOfInjectedStrips;
}

std::vector<Cluster> OTSSAtoSSAecv::produceMatchingPixelClusterList(uint8_t stubRow, uint8_t stubSeed)
{
    std::vector<Cluster> thePixelClusterList;
    thePixelClusterList.push_back(Cluster(stubRow, stubSeed/2 == 1 ? 118 : 0, 2));
    return thePixelClusterList;
}

std::vector<std::vector<Stub>> OTSSAtoSSAecv::producePossibleStubVectorList(const std::vector<Cluster>& thePixelClusterList)
{
    std::vector<int>                                            bendingList{3, 5, 7};
    std::vector<std::vector<Stub>> possibleStubVectorList;
    for(const auto& thePixelCluster: thePixelClusterList)
    {
        for(auto bending: bendingList)
        {
            int                                            multiplier = thePixelCluster.fFirstCol == 0 ? -1 : +1;
            std::vector<Stub> theStubVector{Stub(thePixelCluster.fFirstCol * 2 + thePixelCluster.fColWidth - 1, multiplier * bending, fStubRowCoordinate)};
            possibleStubVectorList.push_back(theStubVector);
        }
    }

    return possibleStubVectorList;
}
