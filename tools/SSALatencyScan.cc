/*!
  \file                  LatencyScanAsync.cc
  \brief                 Implementaion of SCurve scan
  \author                kevin Nash
  \version               1.0
  \date                  28/06/18
  Support:               email to knash@gmail.com
*/

#include "SSALatencyScan.h"

using namespace Ph2_HwDescription;
using namespace Ph2_HwInterface;

SSALatencyScan::SSALatencyScan() : Tool() {}

SSALatencyScan::~SSALatencyScan() {}

void SSALatencyScan::Initialise(void) {}

void SSALatencyScan::run(void)
{
    ReadoutChip* cFirstReadoutChip = static_cast<ReadoutChip*>(fDetectorContainer->at(0)->at(0)->at(0)->at(0));

    cWithSSA = (cFirstReadoutChip->getFrontEndType() == FrontEndType::SSA);
    cWithMPA = (cFirstReadoutChip->getFrontEndType() == FrontEndType::MPA);

    DetectorDataContainer theHitContainer;
    ContainerFactory::copyAndInitChannel<std::pair<std::array<uint32_t, 2>, float>>(*fDetectorContainer, theHitContainer);

    for(auto cBoard: theHitContainer)
    {
        BeBoard* theBeBoard = static_cast<BeBoard*>(fDetectorContainer->at(cBoard->getIndex()));
        if(cWithSSA) theBeBoard->setEventType(EventType::SSA);
        if(cWithMPA) theBeBoard->setEventType(EventType::MPA);

        this->enableTestPulse(true);
        setFWTestPulse();

        if(cWithSSA)
        {
            setSameDacBeBoard(theBeBoard, "Bias_CALDAC", 120);
            setSameDacBeBoard(theBeBoard, "Bias_THDAC", 60);
        }

        if(cWithMPA)
        {
            int mpacal = 170;
            int mpath  = 120;
            setSameDacBeBoard(theBeBoard, "CalDAC0", mpacal);
            setSameDacBeBoard(theBeBoard, "CalDAC1", mpacal);
            setSameDacBeBoard(theBeBoard, "CalDAC2", mpacal);
            setSameDacBeBoard(theBeBoard, "CalDAC3", mpacal);
            setSameDacBeBoard(theBeBoard, "CalDAC4", mpacal);
            setSameDacBeBoard(theBeBoard, "CalDAC5", mpacal);
            setSameDacBeBoard(theBeBoard, "CalDAC6", mpacal);
            setSameDacBeBoard(theBeBoard, "ThDAC0", mpath);
            setSameDacBeBoard(theBeBoard, "ThDAC1", mpath);
            setSameDacBeBoard(theBeBoard, "ThDAC2", mpath);
            setSameDacBeBoard(theBeBoard, "ThDAC3", mpath);
            setSameDacBeBoard(theBeBoard, "ThDAC4", mpath);
            setSameDacBeBoard(theBeBoard, "ThDAC5", mpath);
            setSameDacBeBoard(theBeBoard, "ThDAC6", mpath);
        }

        for(auto cOpticalGroup: *cBoard)
        {
            for(auto cHybrid: *cOpticalGroup)
            {
                for(auto cChip: *cHybrid)
                {
                    ReadoutChip* theChip = static_cast<ReadoutChip*>(fDetectorContainer->at(cBoard->getIndex())->at(cOpticalGroup->getIndex())->at(cHybrid->getIndex())->at(cChip->getIndex()));
                    // this->fReadoutChipInterface->WriteChipReg(theChip, "Bias_CALDAC", 120);
                    // this->fReadoutChipInterface->WriteChipReg(theChip, "ReadoutMode", 0x0);
                    // this->fReadoutChipInterface->WriteChipReg(theChip, "Bias_THDAC", 60);
                    // this->fReadoutChipInterface->WriteChipReg(theChip, "FE_Calibration", 1);
                    // for (int i = 1; i<=120;i++ )
                    //		{
                    //	    this->fReadoutChipInterface->WriteChipReg(theChip, "THTRIMMING_S" + std::to_string(i), 0);
                    //	    this->fReadoutChipInterface->WriteChipReg(theChip, "ENFLAGS_S" + std::to_string(i), 17); //
                    // 17 = 10001 (enable strobe)
                    //	}
                    if(cWithSSA) { this->fReadoutChipInterface->WriteChipReg(theChip, "L1-Latency_MSB", 0x0); }
                    if(cWithMPA)
                    {
                        // this->fReadoutChipInterface->WriteChipReg(theChip, "ReadoutMode", 0x0);
                        this->fReadoutChipInterface->WriteChipReg(theChip, "L1Offset_2_ALL", 0x0);
                        // this->fReadoutChipInterface->WriteChipReg(theChip, "ECM", 0x81);
                    }
                }
            }
        }

        uint32_t bestlat  = 0;
        uint32_t maxcount = 0;

        int Nchans = NSSACHANNELS;
        if(cWithMPA) Nchans = NMPACHANNELS;

        for(uint32_t lat = 0; lat <= 255; lat++)
        {
            for(auto cOpticalGroup: *cBoard)
            {
                for(auto cHybrid: *cOpticalGroup)
                {
                    for(auto cChip: *cHybrid)
                    {
                        ReadoutChip* theChip = static_cast<ReadoutChip*>(fDetectorContainer->at(cBoard->getIndex())->at(cOpticalGroup->getIndex())->at(cHybrid->getIndex())->at(cChip->getIndex()));
                        if(cWithSSA) { this->fReadoutChipInterface->WriteChipReg(theChip, "L1-Latency_LSB", lat); }
                        if(cWithMPA)

                        {
                            this->fReadoutChipInterface->WriteChipReg(theChip, "L1Offset_1_ALL", lat);
                            // this->fReadoutChipInterface->WriteChipReg(theChip, "LatencyRx320", lat);
                        }
                    }
                }
            }
            // static_cast<D19cFWInterface*>(fBeBoardInterface->getFirmwareInterface())->ConfigureTestPulseFSM(50,200,1000,0,1,1);
            // this->enableTestPulse( true );
            // setFWTestPulse();
            this->ReadNEvents(theBeBoard, 200);
            const std::vector<Event*>& eventVector = this->GetEvents(theBeBoard);
            unsigned int               thiscount   = 0;

            for(auto& event: eventVector) // for on events - begin
            {
                for(auto cOpticalGroup: *cBoard)
                {
                    for(auto hybrid: *cOpticalGroup) // for on hybrid - begin
                    {
                        for(auto cSSA: *hybrid) // for on chip - begin
                        {
                            unsigned int channelNumber = 0;
                            if(cWithSSA)
                            {
                                for(int i = 1; i <= Nchans; i++)
                                {
                                    thiscount = thiscount + event->DataBit(hybrid->getId(), cSSA->getId(), channelNumber);
                                    channelNumber++;
                                }
                            }
                            if(cWithMPA)
                            {
                                std::vector<PCluster> pclus = static_cast<D19cMPAEvent*>(event)->GetPixelClusters(hybrid->getId(), cSSA->getId());
                                thiscount += pclus.size();
                                // std::cout<<thiscount<<std::endl;
                            }
                        }
                    }
                }
            }

            if(thiscount > maxcount)
            {
                maxcount = thiscount;
                bestlat  = lat;
            }
            std::cout << "lat " << lat << " cnt " << thiscount << std::endl;
        }
        LOG(INFO) << BOLDRED << "Best Latency Value " << std::hex << bestlat << std::dec << RESET;
        for(auto cOpticalGroup: *cBoard)
        {
            for(auto cHybrid: *cOpticalGroup)
            {
                for(auto cChip: *cHybrid)
                {
                    ReadoutChip* theChip = static_cast<ReadoutChip*>(fDetectorContainer->at(cBoard->getIndex())->at(cOpticalGroup->getIndex())->at(cHybrid->getIndex())->at(cChip->getIndex()));

                    if(cWithSSA) { this->fReadoutChipInterface->WriteChipReg(theChip, "L1-Latency_LSB", bestlat); }

                    if(cWithMPA) { this->fReadoutChipInterface->WriteChipReg(theChip, "L1Offset_1_ALL", bestlat); }
                }
            }
        }
    }
}
