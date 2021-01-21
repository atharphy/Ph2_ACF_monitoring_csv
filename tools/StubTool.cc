#include "StubTool.h"

#ifdef __USE_ROOT__

StubTool::StubTool() {}

void StubTool::Initialize()
{
    // let's start by parsing the settings
    this->parseSettings();

    // we want to keep things simple, so lets get a pointer to a CBC and a pointer to a BeBoard
    // this will facilitate the code as we save a lot of looping
    fBoard = static_cast<BeBoard*>(fDetectorContainer->at(0));
    fCbc   = static_cast<ReadoutChip*>(fBoard->at(0)->at(0)->at(0));

    // we also need a TCanvas
    // std::string cDirectory = ( cmd.foundOption ( "output" ) ) ? cmd.optionValue ( "output" ) : "Results/pulseshape";
    // std::string cDirectory = "Results/pulseshape";
    // if ( !cNoise ) cDirectory += "Commissioning";
    //    //else if ( cNoise ) cDirectory += "NoiseScan";
    //
    //        if ( cNoise )          cDirectory += "NoiseScan";
    //            else if ( cSignalFit ) cDirectory += "SignalFit";
    //                else                   cDirectory += "Commissioning";
    //
    // Tool cTool;
    // cTool.CreateResultDirectory ( cDirectory ,false , true);
    // LOG (INFO) << " AAAAAAAAA file name: "<<fDirectoryName;

    // TFile* ff;
    // ff= TFile::Open ( "out_" + , "RECREATE" );

    fCanvas = new TCanvas("StubTool", "StubTool");
    // fCanvas->Divide (2, 1);

    fCanvas2 = new TCanvas("tpampvsped", "tpampvsped");
    ftpvsped = new TH1F("htpampvsped", "htpampvsped", 500, 0, 500);
    // now let's enable exactly 1 channel by setting the offset to 80 decimal in electron mode or 170 in hole mode
    // the channel registers in the CBC file start from 1, our channels start from 0
    // Jarne: potential problem?

    // We need this?---------------------------
    // this->fReadoutChipInterface->WriteChipReg (fCbc, Form ("Channel%03d", fChan + 1), (fHoleMode) ? 0xaa : 0x50 );

    // fChannel = new Channel (fBoard->getBeId(), fCbc->getFeId(), fCbc->getChipId(), fChan );
    // fChannel->initializeHist (0, "VCth");

    // std::vector<Channel> fChannelVector;
    nChan = 254;
    for(uint8_t iCh = 0; iCh < nChan; iCh++)
    {
        this->fReadoutChipInterface->WriteChipReg(fCbc, Form("Channel%03d", iCh + 1), (fHoleMode) ? 0xaa : 0x50);
        fChannelVector.push_back(new Channel(fBoard->getId(), fBoard->at(0)->getId(), fCbc->getFeId(), fCbc->getId(), iCh));
        fChannelVector.back()->initializeHist(0, "VCth");
    }

    fCanvas3 = new TCanvas("chanvsdel", "chanvsdel");
    fCanvas4 = new TCanvas("STUB_VthVSDel", "STUB_VthVSDel");
    fCanvas5 = new TCanvas("STUB_SCAN_tg", "STUB_SCAN_tg");
    fCanvas6 = new TCanvas("STUB_SCAN_bend", "STUB_SCAN_bend");
}

void StubTool::scanStubs()
{
    std::stringstream outp;
    for(auto cBoard: *fDetectorContainer)
    {
        BeBoard* theBoard = static_cast<BeBoard*>(cBoard);
        for(auto cOpticalGroup: *cBoard)
        {
            for(auto cFe: *cOpticalGroup)
            {
                uint32_t         cFeId      = cFe->getId();
                ModuleContainer& cCbcVector = *cFe;
                const uint8_t    nCBC       = cCbcVector.size();
                for(uint8_t iCBC = 0; iCBC < nCBC; iCBC++) { configureTestPulse(static_cast<ReadoutChip*>(static_cast<ReadoutChip*>(cCbcVector.at(iCBC))), 1); }
                // Uncoment for Bend uncoding 2
                // hSTUB_SCAN_tg = new TH2F(stubscanname_tg.c_str(),stubscanname_tg.c_str(),nChan,0,nChan,16,0,8);
                // hSTUB_SCAN_bend = new
                // TH2F(stubscanname_bend.c_str(),stubscanname_bend.c_str(),nChan,0,nChan,16,-6.75,8.25); Comment for
                // Bend uncoding 4
                double      actualbend        = 3;
                double      binbend4[]        = {-7.25, -6.25, -5.25, -4.25, -3.25, -2.25, -1.25, -0.25, 0.25, 1.25, 2.25, 3.25, 4.25, 5.25, 6.25, 7.25, 8.25};
                std::string stubscanname_tg   = "StubsSCAN_TG_CBC";
                std::string stubscanname_bend = "StubsSCAN_BEND_CBC";
                hSTUB_SCAN_tg                 = new TH2F(stubscanname_tg.c_str(), stubscanname_tg.c_str(), nCBC * 127, 0, nCBC * 127, 16, 0, 8);
                hSTUB_SCAN_bend               = new TH2F(stubscanname_bend.c_str(), stubscanname_bend.c_str(), nCBC * 127, 0, nCBC * 127, 16, binbend4);
                std::vector<std::string> vec_stubscanname_bend_offset(nCBC, "");
                for(uint8_t iCBC = 0; iCBC < nCBC; iCBC++)
                {
                    vec_stubscanname_bend_offset[iCBC] = "StubsSCAN_BEND_OFF_CBC" + std::to_string(iCBC);
                    hSTUB_SCAN_bend_off[iCBC]          = new TH2F(vec_stubscanname_bend_offset[iCBC].c_str(), vec_stubscanname_bend_offset[iCBC].c_str(), 31, -8, 8, 16, binbend4);
                    // hSTUB_SCAN_bend_off[iCBC] = new
                    // TH2F(vec_stubscanname_bend_offset[iCBC].c_str(),vec_stubscanname_bend_offset[iCBC].c_str(),31,-7.5,7.5,16,-6.75,8.25);
                }
                for(uint8_t tg = 0; tg < 8; tg++)
                {
                    // LOG(DEBUG) << GREEN << "Test Group " << +tg << RESET;

                    setInitialOffsets();
                    setSystemTestPulse(40, tg, true, 0); // (testPulseAmplitude, TestGroup, true, holemode )

                    // get channels in test group
                    std::vector<uint8_t> cChannelVector;
                    cChannelVector.clear();
                    cChannelVector = findChannelsInTestGroup(tg);

                    // first, configure test pulse
                    setDelayAndTestGroup(5030, tg);

                    // set the threshold, correlation window (pt), bend decoding in all chips!
                    uint16_t     cVcth                = 500;
                    uint8_t      cVcth1               = cVcth & 0x00FF;
                    uint8_t      cVcth2               = (cVcth & 0x0300) >> 8;
                    unsigned int Pipe_StubSel_Ptwidth = 14;
                    unsigned int BendReg[]            = {153, 170, 187, 204, 221, 238, 255, 16, 33, 50, 67, 84, 101, 118, 135};
                    // unsigned int BendReg[] = {153, 170, 187, 204, 221, 238, 255, 16, 33, 50, 67, 84, 101, 118, 120};
                    for(uint8_t iCBC = 0; iCBC < nCBC; iCBC++)
                    {
                        fReadoutChipInterface->WriteChipReg(static_cast<ReadoutChip*>(cCbcVector.at(iCBC)), "VCth1", cVcth1);
                        fReadoutChipInterface->WriteChipReg(static_cast<ReadoutChip*>(cCbcVector.at(iCBC)), "VCth2", cVcth2);
                        fReadoutChipInterface->WriteChipReg(static_cast<ReadoutChip*>(cCbcVector.at(iCBC)), "Pipe&StubInpSel&Ptwidth", Pipe_StubSel_Ptwidth);
                        for(int ireg = 0; ireg < 15; ireg++) { fReadoutChipInterface->WriteChipReg(static_cast<ReadoutChip*>(cCbcVector.at(iCBC)), "Bend" + std::to_string(ireg), BendReg[ireg]); }
                    }

                    uint8_t     cRegValue;
                    std::string cRegName;

                    for(double offset = (actualbend); offset >= -(actualbend); offset -= 0.5)
                    {
                        // LOG(DEBUG) << GREEN << " !!!  Offset: " << offset << RESET;
                        for(uint8_t iCBC = 0; iCBC < nCBC; iCBC++) { setCorrelationWinodwOffsets(static_cast<ReadoutChip*>(cCbcVector.at(iCBC)), offset, offset, offset, offset); }

                        // Unmasking desired channels
                        for(unsigned int iChan = 0; iChan < nChan; iChan++)
                        {
                            if(iChan % 48 != 0) continue;
                            // LOG(DEBUG) << "Looking at Channel " << iChan;
                            for(uint8_t iCBC = 0; iCBC < nCBC; iCBC++)
                            {
                                for(unsigned int i = 0; i < fChannelMaskMapCBC3.size(); i++)
                                {
                                    static_cast<ReadoutChip*>(cCbcVector.at(iCBC))->setReg(fChannelMaskMapCBC3[i], 0);
                                    cRegValue = static_cast<ReadoutChip*>(cCbcVector.at(iCBC))->getReg(fChannelMaskMapCBC3[i]);
                                    cRegName  = fChannelMaskMapCBC3[i];
                                    fReadoutChipInterface->WriteChipReg(static_cast<ReadoutChip*>(cCbcVector.at(iCBC)), cRegName, cRegValue);
                                    // LOG (INFO) << fChannelMaskMapCBC3[i] << " " << std::bitset<8> (cRegValue);
                                }
                                // for ( unsigned int smg = 0; smg < 6; smg+=2) //only using masks
                                for(unsigned int smg = 0; smg < 3; smg++)
                                {
                                    // if (tg > 3 && smg == 0) smg++; //only using masks
                                    // if ( (mg+smg) >= 32) continue;
                                    for(int i = 1; i <= 2; i++)
                                    {
                                        if((iChan + (smg * 16) + (tg * 2) + i) > nChan) continue;
                                        maskChannel(static_cast<ReadoutChip*>(cCbcVector.at(iCBC)), (iChan + (smg * 16) + (tg * 2) + i), false);
                                        LOG(DEBUG) << "Unmasking : " << +(iChan + (smg * 16) + (tg * 2) + i) << " = " << +iChan << " + (" << +smg << ")*16 +" << +((tg * 2) + i);
                                    }
                                }

                                // CHECKING CBC REGISTERS
                                // CheckCbcReg(static_cast<ReadoutChip*>(cCbcVector.at(iCBC)));

                                // now read Events
                                ReadNEvents(theBoard, fNevents);
                                const std::vector<Event*> cEvents    = GetEvents(theBoard);
                                int                       countEvent = 0;
                                for(auto cEvent: cEvents)
                                {
                                    ++countEvent;
                                    LOG(DEBUG) << "Event : " << countEvent << " !";
                                    std::vector<uint32_t> cHits = cEvent->GetHits(cFeId, iCBC);
                                    unsigned              nHits = 0;
                                    if(cHits.size() == nChan)
                                        LOG(INFO) << RED << "All channels firing in CBC" << +iCBC << "!!" << RESET;
                                    else
                                    {
                                        // LOG(DEBUG) << BLUE << "List of hits: " << RESET;
                                        for(uint32_t cHit: cHits)
                                        {
                                            nHits++;
                                            double HIT   = cHit + 1;
                                            double STRIP = (HIT / 2);
                                            LOG(DEBUG) << BLUE << std::dec << cHit << " : " << HIT << " , " << STRIP << RESET;
                                            if(offset == 0 && (int)HIT % 2 != 0) hSTUB_SCAN_tg->Fill(STRIP + (iCBC * 127), tg + 0.5, 0.5);
                                        }
                                    }
                                    if(cEvent->StubBit(cFeId, static_cast<ReadoutChip*>(cCbcVector.at(iCBC))->getChipId()))
                                    {
                                        uint8_t stubCounter = 1;
                                        for(auto& cStub: cEvent->StubVector(cFeId, static_cast<ReadoutChip*>(cCbcVector.at(iCBC))->getChipId()))
                                        {
                                            double stub_position = cStub.getPosition();
                                            double stub_bend     = Decoding_stub4(cStub.getBend());
                                            double stub_strip    = cStub.getCenter();

                                            double expect       = (iChan + tg * 2 + ((stubCounter - 1) * 16));
                                            double expect_strip = (((expect + 2) / 2) - 1);
                                            LOG(DEBUG) << "CBC" << +iCBC << " , Stub: " << +stubCounter << " | Position: " << stub_position << " , EXPECT POS : " << +(expect + 2)
                                                       << " | Bend: " << std::bitset<4>(cStub.getBend()) << " -> " << stub_bend << " , EXPECT BEND: " << -(offset) << " || Strip: " << stub_strip
                                                       << " , EXPECTED STRIP : " << +(expect_strip) << " , Filling STRIP: " << +(stub_strip + (iCBC * 127));

                                            stubCounter++;
                                            hSTUB_SCAN_bend_off[iCBC]->Fill(offset, stub_bend);
                                            hSTUB_SCAN_bend->Fill(stub_strip + (iCBC * 127), stub_bend);
                                            fCanvas6->cd();
                                            hSTUB_SCAN_bend->Draw("COLZ");
                                            hSTUB_SCAN_bend->SetStats(0);
                                            fCanvas6->Update();

                                            if(offset != 0) continue;
                                            hSTUB_SCAN_tg->Fill(stub_strip + (iCBC * 127), tg);
                                            fCanvas5->cd();
                                            hSTUB_SCAN_tg->Draw("COLZ");
                                            hSTUB_SCAN_tg->SetStats(0);
                                            fCanvas5->Update();
                                        }
                                    }
                                    else
                                        LOG(DEBUG) << RED << "!!!!! NO STUB in CBC" << +iCBC << " | Event : " << +countEvent << " !!!!!!!!!" << RESET;
                                }
                            }
                        }
                    } // correlation window offset
                }

                hSTUB_SCAN_tg->Write();
                hSTUB_SCAN_bend->Write();
                for(uint8_t iCBC = 0; iCBC < nCBC; iCBC++) { hSTUB_SCAN_bend_off[iCBC]->Write(); }
                for(uint8_t iCBC = 0; iCBC < nCBC; iCBC++) { configureTestPulse(static_cast<ReadoutChip*>(cCbcVector.at(iCBC)), 0); }
            }
        }
    }
}

void StubTool::scanStubs_wNoise()
{
    std::stringstream outp;
    for(auto cBoard: *fDetectorContainer)
    {
        BeBoard* theBoard = static_cast<BeBoard*>(cBoard);
        for(auto cOpticalGroup: *cBoard)
        {
            for(auto cFe: *cOpticalGroup)
            {
                uint32_t         cFeId      = cFe->getId();
                ModuleContainer& cCbcVector = *cFe;
                const uint8_t    nCBC       = cCbcVector.size();
                // Uncoment for Bend uncoding 2
                // hSTUB_SCAN_tg = new TH2F(stubscanname_tg.c_str(),stubscanname_tg.c_str(),nChan,0,nChan,16,0,8);
                // hSTUB_SCAN_bend = new
                // TH2F(stubscanname_bend.c_str(),stubscanname_bend.c_str(),nChan,0,nChan,16,-6.75,8.25); Comment for
                // Bend uncoding 4
                double      actualbend         = 7;
                double      binbend4[]         = {-7.25, -6.25, -5.25, -4.25, -3.25, -2.25, -1.25, -0.25, 0.25, 1.25, 2.25, 3.25, 4.25, 5.25, 6.25, 7.25, 8.25};
                std::string stubscanname_tg    = "StubsSCAN_TG_CBC";
                std::string stubscanname_bend  = "StubsSCAN_BEND_CBC";
                std::string stubscanname_error = "StubsSCAN_BEND_ERRORS";
                hSTUB_SCAN_tg                  = new TH2F(stubscanname_tg.c_str(), stubscanname_tg.c_str(), nCBC * 127, 0, nCBC * 127, 16, 0, 8);
                hSTUB_SCAN_bend                = new TH2F(stubscanname_bend.c_str(), stubscanname_bend.c_str(), nCBC * 127, 0, nCBC * 127, 16, binbend4);
                hSTUB_SCAN_error               = new TH2F(stubscanname_error.c_str(), stubscanname_error.c_str(), nCBC * 127, 0, nCBC * 127, 16, binbend4);
                std::vector<std::string> vec_stubscanname_bend_offset(nCBC, "");
                for(uint8_t iCBC = 0; iCBC < nCBC; iCBC++)
                {
                    vec_stubscanname_bend_offset[iCBC] = "StubsSCAN_BEND_OFF_CBC" + std::to_string(iCBC);
                    hSTUB_SCAN_bend_off[iCBC]          = new TH2F(vec_stubscanname_bend_offset[iCBC].c_str(), vec_stubscanname_bend_offset[iCBC].c_str(), 31, -8, 8, 16, binbend4);
                    // hSTUB_SCAN_bend_off[iCBC] = new
                    // TH2F(vec_stubscanname_bend_offset[iCBC].c_str(),vec_stubscanname_bend_offset[iCBC].c_str(),31,-7.5,7.5,16,-6.75,8.25);
                }
                for(uint8_t tg = 0; tg < 8; tg++)
                {
                    LOG(DEBUG) << GREEN << "Test Group " << +tg << RESET;
                    setInitialOffsets();
                    // get channels in test group
                    std::vector<uint8_t> cChannelVector;
                    cChannelVector.clear();
                    cChannelVector = findChannelsInTestGroup(tg);

                    // set the threshold, correlation window (pt), bend decoding in all chips!
                    uint16_t         cVcth = 700;
                    ThresholdVisitor cThresholdVisitor(fReadoutChipInterface, cVcth);
                    this->accept(cThresholdVisitor);
                    unsigned int Pipe_StubSel_Ptwidth = 14;
                    unsigned int BendReg[]            = {153, 170, 187, 204, 221, 238, 255, 16, 33, 50, 67, 84, 101, 118, 135};
                    // unsigned int BendReg[] = {153, 170, 187, 204, 221, 238, 255, 16, 33, 50, 67, 84, 101, 118, 120};
                    std::vector<std::pair<std::string, uint16_t>> cRegVec;
                    for(uint8_t iCBC = 0; iCBC < nCBC; iCBC++)
                    {
                        cRegVec.push_back(std::make_pair("Pipe&StubInpSel&Ptwidth", Pipe_StubSel_Ptwidth));
                        for(int ireg = 0; ireg < 15; ireg++) { cRegVec.push_back(std::make_pair("Bend" + std::to_string(ireg), BendReg[ireg])); }
                        fReadoutChipInterface->WriteChipMultReg(static_cast<ReadoutChip*>(cCbcVector.at(iCBC)), cRegVec);
                        cRegVec.clear();
                    }

                    for(double bend = -(actualbend * 2); bend <= (actualbend * 2); bend += 1)
                    { // step of halves: 2 halves = 1 strip
                        uint8_t     cRegValue;
                        std::string cRegName;
                        // Unmasking desired channels
                        for(unsigned int iChan = 0; iChan < nChan; iChan++)
                        {
                            if(iChan % 96 != 0) continue;
                            {
                                for(uint8_t step = 0; step <= 1; step++)
                                {
                                    for(uint8_t iCBC = 0; iCBC < nCBC; iCBC++)
                                    {
                                        // MASKING ALL CHANNELS
                                        cRegVec.clear();
                                        for(unsigned int i = 0; i < fChannelMaskMapCBC3.size(); i++)
                                        {
                                            static_cast<ReadoutChip*>(cCbcVector.at(iCBC))->setReg(fChannelMaskMapCBC3[i], 0);
                                            cRegValue = static_cast<ReadoutChip*>(cCbcVector.at(iCBC))->getReg(fChannelMaskMapCBC3[i]);
                                            cRegName  = fChannelMaskMapCBC3[i];
                                            cRegVec.push_back(std::make_pair(cRegName, cRegValue));
                                            // LOG (DEBUG) << fChannelMaskMapCBC3[i] << " " << std::bitset<8>
                                            // (cRegValue);
                                        }
                                        fReadoutChipInterface->WriteChipMultReg(static_cast<ReadoutChip*>(cCbcVector.at(iCBC)), cRegVec);
                                        cRegVec.clear();
                                    }

                                    int seedChan[3] = {-1000};
                                    LOG(DEBUG) << "--------------";
                                    for(unsigned int smg = 0; smg < 3; smg++)
                                    {
                                        seedChan[smg] = (iChan + (smg * 32) + (step * 16) + (tg * 2) + 1);
                                        if(seedChan[smg] < 0 || seedChan[smg] > nChan) continue;
                                        for(uint8_t iCBC = 0; iCBC < nCBC; iCBC++)
                                        {
                                            if((seedChan[smg] + bend - 1) < 0)
                                            {
                                                if(iCBC == 0) continue;
                                                LOG(DEBUG) << "Channel (CBC" << +(iCBC) << "): " << +seedChan[smg];
                                                int corrChan = nChan + (seedChan[smg] + bend + 1);
                                                if((int)bend % 2 != 0) corrChan = nChan + (seedChan[smg] + bend);
                                                if(corrChan % 2 != 0 || corrChan <= 0) continue;
                                                maskChannel(static_cast<ReadoutChip*>(cCbcVector.at(iCBC - 1)), corrChan, false);
                                                maskChannel(static_cast<ReadoutChip*>(cCbcVector.at(iCBC)), seedChan[smg], false);
                                                LOG(DEBUG) << "Cond 1: Corr Chan (CBC" << +(iCBC - 1) << "): " << +corrChan << " (" << +(seedChan[smg] + bend - 1) << ")";
                                                if((int)bend % 2 != 0 && corrChan + 2 > 0)
                                                {
                                                    maskChannel(static_cast<ReadoutChip*>(cCbcVector.at(iCBC - 1)), corrChan + 2, false);
                                                    LOG(DEBUG) << "              , Corr Chan : " << (corrChan + 2);
                                                }
                                            }
                                            else if((seedChan[smg] + bend + 1) > nChan)
                                            {
                                                if(iCBC == nCBC - 1) continue;
                                                LOG(DEBUG) << "Channel (CBC" << +iCBC << "): " << seedChan[smg];
                                                int corrChan = (seedChan[smg] + bend + 1) - nChan;
                                                if((int)bend % 2 != 0) corrChan = (seedChan[smg] + bend) - nChan;
                                                if(corrChan % 2 != 0) continue;
                                                maskChannel(static_cast<ReadoutChip*>(cCbcVector.at(iCBC)), seedChan[smg], false);
                                                if(corrChan > 0)
                                                    maskChannel(static_cast<ReadoutChip*>(cCbcVector.at(iCBC + 1)), corrChan, false);
                                                else if(corrChan == 0)
                                                {
                                                    corrChan = nChan;
                                                    maskChannel(static_cast<ReadoutChip*>(cCbcVector.at(iCBC)), corrChan, false);
                                                }
                                                LOG(DEBUG) << "Cond 2: Corr Chan (" << +(iCBC) << "/" << +(iCBC + 1) << "): " << corrChan << " (" << seedChan[smg] + bend + 1 << ")";
                                                if(corrChan == nChan)
                                                {
                                                    corrChan = 2;
                                                    maskChannel(static_cast<ReadoutChip*>(cCbcVector.at(iCBC + 1)), corrChan, false);
                                                    LOG(DEBUG) << "                 , Corr Chan " << +(iCBC + 1) << ": " << (corrChan);
                                                }
                                                else if((int)bend % 2 != 0 && corrChan + 2 > 0)
                                                {
                                                    maskChannel(static_cast<ReadoutChip*>(cCbcVector.at(iCBC + 1)), corrChan + 2, false);
                                                    LOG(DEBUG) << "                 , Corr Chan " << +(iCBC + 1) << ": " << (corrChan + 2);
                                                }
                                            }
                                            else
                                            {
                                                LOG(DEBUG) << "Channel (CBC" << +iCBC << "): " << seedChan[smg];
                                                int corrChan = (seedChan[smg] + bend + 1);
                                                if((int)bend % 2 != 0) corrChan = (seedChan[smg] + bend);
                                                if(corrChan % 2 != 0 || corrChan <= 0) continue;
                                                maskChannel(static_cast<ReadoutChip*>(cCbcVector.at(iCBC)), seedChan[smg], false);
                                                maskChannel(static_cast<ReadoutChip*>(cCbcVector.at(iCBC)), corrChan, false);
                                                LOG(DEBUG) << "Cond 3: Corr Chan (CBC" << +(iCBC) << "): " << corrChan << " (" << seedChan[smg] + bend + 1 << ")";
                                                if((int)bend % 2 != 0 && corrChan + 2 > 0)
                                                {
                                                    maskChannel(static_cast<ReadoutChip*>(cCbcVector.at(iCBC)), corrChan + 2, false);
                                                    LOG(DEBUG) << "         , Corr Chan : " << (corrChan + 2);
                                                }
                                            }
                                            // MASKING BAD CHANNELS FOR CBC3.0
                                            /*if (iCBC > 0)
                                            {
                                              maskChannel(static_cast<ReadoutChip*>(cCbcVector.at(iCBC-1)), 107, true);
                                              maskChannel(static_cast<ReadoutChip*>(cCbcVector.at(iCBC-1)), 225, true);
                                            }
                                            maskChannel(static_cast<ReadoutChip*>(cCbcVector.at(iCBC)), 107, true);
                                            maskChannel(static_cast<ReadoutChip*>(cCbcVector.at(iCBC)), 225, true);
                                            */
                                        }
                                    }

                                    // CHECKING CBC REGISTERS
                                    // for (uint8_t iCBC = 0; i< nCBC; iCBC++)
                                    // {CheckCbcReg(static_cast<ReadoutChip*>(cCbcVector.at(iCBC)))};

                                    // now read Events
                                    ReadNEvents(theBoard, fNevents);
                                    const std::vector<Event*> cEvents    = GetEvents(theBoard);
                                    int                       countEvent = 0;
                                    for(auto cEvent: cEvents)
                                    {
                                        ++countEvent;
                                        for(uint8_t iCBC = 0; iCBC < nCBC; iCBC++)
                                        {
                                            std::vector<uint32_t> cHits = cEvent->GetHits(cFeId, static_cast<ReadoutChip*>(cCbcVector.at(iCBC))->getChipId());
                                            if(cHits.size() == nChan)
                                                LOG(INFO) << RED << "CBC " << +iCBC << ": All channels firing!" << RESET;
                                            else
                                            {
                                                LOG(DEBUG) << BLUE << "List of hits CBC" << +iCBC << ": " << RESET;
                                                for(uint32_t cHit: cHits)
                                                {
                                                    double HIT   = cHit + 1;
                                                    double STRIP = floor((HIT - 1) / 2);
                                                    LOG(DEBUG) << BLUE << std::dec << cHit << " : " << HIT << " , " << STRIP << RESET;
                                                    if(bend == 0 && (int)HIT % 2 != 0) hSTUB_SCAN_tg->Fill(STRIP + (iCBC * 127), tg + 0.5, 0.5);
                                                }
                                            }
                                            for(uint8_t smg = 0; smg < 3; smg++)
                                            {
                                                if(seedChan[smg] < 0 || seedChan[smg] > nChan) continue;
                                                if(((seedChan[smg] - 1) / 2) + (iCBC * 127) + (bend / 2) < 0 || ((seedChan[smg - 1]) / 2) + (iCBC * 127) + (bend / 2) >= nChan) continue;
                                                if(cEvent->StubBit(cFeId, static_cast<ReadoutChip*>(cCbcVector.at(iCBC))->getChipId()))
                                                {
                                                    bool stubfinder = false;
                                                    for(auto& cStub: cEvent->StubVector(cFeId, static_cast<ReadoutChip*>(cCbcVector.at(iCBC))->getChipId()))
                                                    {
                                                        if(((seedChan[smg] - 1) / 2) + (iCBC * 127) == (cStub.getCenter() + (iCBC * 127)))
                                                        {
                                                            stubfinder = true;
                                                            if(abs((bend / 2) - Decoding_stub4(cStub.getBend())) > 0.25)
                                                            {
                                                                LOG(INFO) << RED << "Wrong stub bend at " << +(((seedChan[smg] - 1) / 2) + (iCBC * 127)) << " | Expected: " << +(bend / 2)
                                                                          << ", measured: " << +Decoding_stub4(cStub.getBend()) << RESET;
                                                                hSTUB_SCAN_error->Fill(((seedChan[smg] - 1) / 2) + (iCBC * 127), (bend / 2));
                                                            }
                                                            break;
                                                        }
                                                    }
                                                    if(!stubfinder)
                                                    {
                                                        LOG(INFO) << RED << "Missing CBC HIT at " << +(((seedChan[smg] - 1) / 2) + (iCBC * 127)) << RESET;
                                                        hSTUB_SCAN_error->Fill(((seedChan[smg] - 1) / 2) + (iCBC * 127), (bend / 2));
                                                    }
                                                }
                                                else
                                                {
                                                    LOG(INFO) << RED << "NO CBC DATA at " << +(((seedChan[smg] - 1) / 2) + (iCBC * 127)) << RESET;
                                                    hSTUB_SCAN_error->Fill(((seedChan[smg] - 1) / 2) + (iCBC * 127), (bend / 2));
                                                }
                                            }
                                            if(cEvent->StubBit(cFeId, static_cast<ReadoutChip*>(cCbcVector.at(iCBC))->getChipId()))
                                            {
                                                uint8_t stubCounter = 0;
                                                for(auto& cStub: cEvent->StubVector(cFeId, static_cast<ReadoutChip*>(cCbcVector.at(iCBC))->getChipId()))
                                                {
                                                    stubCounter++;
                                                    double stub_position = cStub.getPosition();
                                                    double stub_bend     = Decoding_stub4(cStub.getBend());
                                                    double stub_strip    = cStub.getCenter();
                                                    LOG(DEBUG) << "CBC" << +iCBC << " , Stub: " << +(stubCounter) << " | Position: " << stub_position
                                                               << " , EXPECT POS : " << +(seedChan[stubCounter - 1] + 1) << " | Bend: " << std::bitset<4>(cStub.getBend()) << " -> " << stub_bend
                                                               << " , EXPECT BEND: " << +(bend / 2) << " || Strip: " << stub_strip
                                                               << " , EXPECTED STRIP : " << (((seedChan[stubCounter - 1] - 1) / 2) + (iCBC * 127))
                                                               << " , Filling STRIP: " << +(stub_strip + (iCBC * 127));
                                                    hSTUB_SCAN_bend_off[iCBC]->Fill(bend / 2, stub_bend);
                                                    hSTUB_SCAN_bend->Fill(stub_strip + (iCBC * 127), stub_bend);
                                                    fCanvas6->cd();
                                                    hSTUB_SCAN_bend->Draw("COLZ");
                                                    hSTUB_SCAN_bend->SetStats(0);
                                                    fCanvas6->Update();

                                                    if(bend != 0) continue;
                                                    hSTUB_SCAN_tg->Fill(stub_strip + (iCBC * 127), tg);
                                                    fCanvas5->cd();
                                                    hSTUB_SCAN_tg->Draw("COLZ");
                                                    hSTUB_SCAN_tg->SetStats(0);
                                                    fCanvas5->Update();
                                                } // end of stub loop
                                            }     // enf of stub condition
                                            else
                                                LOG(DEBUG) << RED << "!!!!! NO STUB in CBC" << +iCBC << " | Event : " << +countEvent << " !!!!!!!!!" << RESET;
                                        } // end of CBC loop
                                    }     // end event loop
                                }         // end of skip 16 channels loop
                            }             // end of step of 96 channels loop
                        }                 // end of channel loop
                    }                     // end bend loop
                }                         // end of TG loop
                hSTUB_SCAN_tg->Write();
                hSTUB_SCAN_bend->Write();
                hSTUB_SCAN_error->Write();
                for(uint8_t iCBC = 0; iCBC < nCBC; iCBC++) { hSTUB_SCAN_bend_off[iCBC]->Write(); }
            }
        }
    }
}

void StubTool::scanStubs_swap()
{
    std::stringstream outp;
    for(auto cBoard: *fDetectorContainer)
    {
        BeBoard* theBoard = static_cast<BeBoard*>(cBoard);
        for(auto cOpticalGroup: *cBoard)
        {
            for(auto cFe: *cOpticalGroup)
            {
                uint32_t         cFeId      = cFe->getId();
                ModuleContainer& cCbcVector = *cFe;
                const uint8_t    nCBC       = cCbcVector.size();
                // Uncoment for Bend uncoding 2
                // hSTUB_SCAN_tg = new TH2F(stubscanname_tg.c_str(),stubscanname_tg.c_str(),nChan,0,nChan,16,0,8);
                // hSTUB_SCAN_bend = new
                // TH2F(stubscanname_bend.c_str(),stubscanname_bend.c_str(),nChan,0,nChan,16,-6.75,8.25); Comment for
                // Bend uncoding 4
                double      actualbend         = 7;
                double      binbend4[]         = {-7.25, -6.25, -5.25, -4.25, -3.25, -2.25, -1.25, -0.25, 0.25, 1.25, 2.25, 3.25, 4.25, 5.25, 6.25, 7.25, 8.25};
                std::string stubscanname_tg    = "StubsSCAN_TG_CBC";
                std::string stubscanname_bend  = "StubsSCAN_BEND_CBC";
                std::string stubscanname_error = "StubsSCAN_BEND_ERRORS";
                hSTUB_SCAN_tg                  = new TH2F(stubscanname_tg.c_str(), stubscanname_tg.c_str(), nCBC * 127, 0, nCBC * 127, 16, 0, 8);
                hSTUB_SCAN_bend                = new TH2F(stubscanname_bend.c_str(), stubscanname_bend.c_str(), nCBC * 127, 0, nCBC * 127, 16, binbend4);
                hSTUB_SCAN_error               = new TH2F(stubscanname_error.c_str(), stubscanname_error.c_str(), nCBC * 127, 0, nCBC * 127, 16, binbend4);
                std::vector<std::string> vec_stubscanname_bend_offset(nCBC, "");
                for(uint8_t iCBC = 0; iCBC < nCBC; iCBC++)
                {
                    vec_stubscanname_bend_offset[iCBC] = "StubsSCAN_BEND_OFF_CBC" + std::to_string(iCBC);
                    hSTUB_SCAN_bend_off[iCBC]          = new TH2F(vec_stubscanname_bend_offset[iCBC].c_str(), vec_stubscanname_bend_offset[iCBC].c_str(), 31, -8, 8, 16, binbend4);
                    // hSTUB_SCAN_bend_off[iCBC] = new
                    // TH2F(vec_stubscanname_bend_offset[iCBC].c_str(),vec_stubscanname_bend_offset[iCBC].c_str(),31,-7.5,7.5,16,-6.75,8.25);
                }
                for(uint8_t tg = 0; tg < 8; tg++)
                {
                    LOG(DEBUG) << GREEN << "Test Group " << +tg << RESET;

                    setInitialOffsets();
                    // get channels in test group
                    std::vector<uint8_t> cChannelVector;
                    cChannelVector.clear();
                    cChannelVector = findChannelsInTestGroup(tg);

                    // set the threshold, correlation window (pt), bend decoding in all chips!
                    uint16_t         cVcth = 700;
                    ThresholdVisitor cThresholdVisitor(fReadoutChipInterface, cVcth);
                    this->accept(cThresholdVisitor);
                    unsigned int Pipe_StubSel_Ptwidth = 14;
                    unsigned int LayerSwap_CluWidth   = 12;
                    unsigned int BendReg[]            = {153, 170, 187, 204, 221, 238, 255, 16, 33, 50, 67, 84, 101, 118, 135};
                    // unsigned int BendReg[] = {153, 170, 187, 204, 221, 238, 255, 16, 33, 50, 67, 84, 101, 118, 120};
                    std::vector<std::pair<std::string, uint16_t>> cRegVec;
                    for(uint8_t iCBC = 0; iCBC < nCBC; iCBC++)
                    {
                        cRegVec.push_back(std::make_pair("Pipe&StubInpSel&Ptwidth", Pipe_StubSel_Ptwidth));
                        cRegVec.push_back(std::make_pair("LayerSwap&CluWidth", LayerSwap_CluWidth));
                        for(int ireg = 0; ireg < 15; ireg++) { cRegVec.push_back(std::make_pair("Bend" + std::to_string(ireg), BendReg[ireg])); }
                        fReadoutChipInterface->WriteChipMultReg(static_cast<ReadoutChip*>(cCbcVector.at(iCBC)), cRegVec);
                        cRegVec.clear();
                    }

                    for(double bend = -(actualbend * 2); bend <= (actualbend * 2); bend += 1)
                    { // step of halves: 2 halves = 1 strip
                        LOG(DEBUG) << GREEN << " !!!  Actual Bend: " << +(bend / 2) << "  !!! " << RESET;
                        uint8_t     cRegValue;
                        std::string cRegName;
                        // Unmasking desired channels
                        for(unsigned int iChan = 0; iChan < nChan; iChan++)
                        {
                            if(iChan % 96 != 0) continue;
                            {
                                for(uint8_t step = 0; step <= 1; step++)
                                {
                                    for(uint8_t iCBC = 0; iCBC < nCBC; iCBC++)
                                    {
                                        // MASKING ALL CHANNELS
                                        cRegVec.clear();
                                        for(unsigned int i = 0; i < fChannelMaskMapCBC3.size(); i++)
                                        {
                                            static_cast<ReadoutChip*>(cCbcVector.at(iCBC))->setReg(fChannelMaskMapCBC3[i], 0);
                                            cRegValue = static_cast<ReadoutChip*>(cCbcVector.at(iCBC))->getReg(fChannelMaskMapCBC3[i]);
                                            cRegName  = fChannelMaskMapCBC3[i];
                                            cRegVec.push_back(std::make_pair(cRegName, cRegValue));
                                        }
                                        fReadoutChipInterface->WriteChipMultReg(static_cast<ReadoutChip*>(cCbcVector.at(iCBC)), cRegVec);
                                        cRegVec.clear();
                                    }

                                    int seedChan[3] = {-1000};
                                    LOG(DEBUG) << "--------------";
                                    for(unsigned int smg = 0; smg < 3; smg++)
                                    {
                                        seedChan[smg] = (iChan + (smg * 32) + (step * 16) + (tg * 2) + 2);
                                        if(seedChan[smg] < 0 || seedChan[smg] > nChan) continue;
                                        for(uint8_t iCBC = 0; iCBC < nCBC; iCBC++)
                                        {
                                            // LOG (DEBUG) << "CBC " << +(iCBC) << " , Seed = " << +seedChan[smg] << " ,
                                            // bend = " << +bend;
                                            if((seedChan[smg] + bend - 2) < 0)
                                            {
                                                if(iCBC == 0) continue;
                                                LOG(DEBUG) << "Channel (CBC" << +(iCBC) << "): " << +seedChan[smg];
                                                int corrChan = nChan + (seedChan[smg] + bend - 1);
                                                if((int)bend % 2 != 0) corrChan = nChan + (seedChan[smg] + bend);
                                                if(corrChan % 2 == 0 || corrChan <= 0) continue;
                                                maskChannel(static_cast<ReadoutChip*>(cCbcVector.at(iCBC - 1)), corrChan, false);
                                                maskChannel(static_cast<ReadoutChip*>(cCbcVector.at(iCBC)), seedChan[smg], false);
                                                LOG(DEBUG) << "Cond 1: Corr Chan (CBC" << +(iCBC - 1) << "): " << +corrChan;
                                                if((int)bend % 2 != 0 && corrChan - 2 > 0)
                                                {
                                                    maskChannel(static_cast<ReadoutChip*>(cCbcVector.at(iCBC - 1)), corrChan - 2, false);
                                                    LOG(DEBUG) << "              , Corr Chan : " << (corrChan - 2);
                                                }
                                            }
                                            else if((seedChan[smg] + bend) > nChan)
                                            {
                                                if(iCBC == nCBC - 1) continue;
                                                LOG(DEBUG) << "Channel (CBC" << +iCBC << "): " << seedChan[smg];
                                                int corrChan = (seedChan[smg] + bend - 1) - nChan;
                                                if((int)bend % 2 != 0) corrChan = (seedChan[smg] + bend) - nChan;
                                                if(corrChan % 2 == 0) continue;
                                                maskChannel(static_cast<ReadoutChip*>(cCbcVector.at(iCBC)), seedChan[smg], false);
                                                if(corrChan > 0)
                                                {
                                                    maskChannel(static_cast<ReadoutChip*>(cCbcVector.at(iCBC + 1)), corrChan, false);
                                                    LOG(DEBUG) << "Cond 2 v1: Corr Chan (CBC" << iCBC + 1 << "): " << +corrChan;
                                                }
                                                if((int)bend % 2 != 0)
                                                {
                                                    if(corrChan - 2 > 0)
                                                    {
                                                        maskChannel(static_cast<ReadoutChip*>(cCbcVector.at(iCBC + 1)), corrChan - 2, false);
                                                        LOG(DEBUG) << "                 , Corr Chan (CBC" << +(iCBC + 1) << "): " << +(corrChan - 2);
                                                    }
                                                    else
                                                    {
                                                        corrChan = nChan - 1;
                                                        maskChannel(static_cast<ReadoutChip*>(cCbcVector.at(iCBC)), corrChan, false);
                                                        LOG(DEBUG) << "                 , Corr Chan (CBC" << +(iCBC) << "): " << +(corrChan);
                                                    }
                                                }
                                            }
                                            else
                                            {
                                                LOG(DEBUG) << "Channel (CBC" << +iCBC << "): " << seedChan[smg];
                                                int corrChan = (seedChan[smg] + bend - 1);
                                                if((int)bend % 2 != 0) corrChan = (seedChan[smg] + bend);
                                                if(corrChan % 2 == 0 || corrChan <= 0) continue;
                                                maskChannel(static_cast<ReadoutChip*>(cCbcVector.at(iCBC)), seedChan[smg], false);
                                                maskChannel(static_cast<ReadoutChip*>(cCbcVector.at(iCBC)), corrChan, false);
                                                LOG(DEBUG) << "Cond 3: Corr Chan (CBC" << +(iCBC) << "): " << corrChan;
                                                if((int)bend % 2 != 0 && corrChan - 2 > 0)
                                                {
                                                    maskChannel(static_cast<ReadoutChip*>(cCbcVector.at(iCBC)), corrChan - 2, false);
                                                    LOG(DEBUG) << "         , Corr Chan : " << (corrChan - 2);
                                                }
                                            }
                                            // MASKING BAD CHANNELS FOR CBC3.0
                                            /*if (iCBC > 0)
                                            {
                                              maskChannel(static_cast<ReadoutChip*>(cCbcVector.at(iCBC-1)), 108, true);
                                              maskChannel(static_cast<ReadoutChip*>(cCbcVector.at(iCBC-1)), 226, true);
                                            }
                                            maskChannel(static_cast<ReadoutChip*>(cCbcVector.at(iCBC)), 108, true);
                                            maskChannel(static_cast<ReadoutChip*>(cCbcVector.at(iCBC)), 226, true);
                                            */
                                        }
                                    }

                                    // CHECKING CBC REGISTERS
                                    // for (uint8_t iCBC = 0; iCBC< nCBC; iCBC++)
                                    // {CheckCbcReg(static_cast<ReadoutChip*>(cCbcVector.at(iCBC)));}

                                    // now read Events
                                    ReadNEvents(theBoard, fNevents);
                                    const std::vector<Event*> cEvents    = GetEvents(theBoard);
                                    int                       countEvent = 0;
                                    for(auto cEvent: cEvents)
                                    {
                                        ++countEvent;
                                        for(uint8_t iCBC = 0; iCBC < nCBC; iCBC++)
                                        {
                                            std::vector<uint32_t> cHits = cEvent->GetHits(cFeId, static_cast<ReadoutChip*>(cCbcVector.at(iCBC))->getChipId());
                                            if(cHits.size() == nChan)
                                                LOG(INFO) << RED << "CBC " << +iCBC << ": All channels firing!" << RESET;
                                            else
                                            {
                                                LOG(DEBUG) << BLUE << "List of hits CBC" << +iCBC << ": " << RESET;
                                                for(uint32_t cHit: cHits)
                                                {
                                                    double HIT   = cHit + 1;
                                                    double STRIP = floor((HIT - 1) / 2);
                                                    LOG(DEBUG) << BLUE << std::dec << cHit << " : " << HIT << " , " << STRIP << RESET;
                                                    if(bend == 0 && (int)HIT % 2 != 0) hSTUB_SCAN_tg->Fill(STRIP + (iCBC * 127), tg + 0.5, 0.5);
                                                }
                                            }
                                            for(uint8_t smg = 0; smg < 3; smg++)
                                            {
                                                if(seedChan[smg] < 0 || seedChan[smg] > nChan) continue;
                                                if(((seedChan[smg] - 1) / 2) + (iCBC * 127) + (bend / 2) < 0 || ((seedChan[smg - 1]) / 2) + (iCBC * 127) + (bend / 2) >= nChan) continue;
                                                if(cEvent->StubBit(cFeId, static_cast<ReadoutChip*>(cCbcVector.at(iCBC))->getChipId()))
                                                {
                                                    bool stubfinder = false;
                                                    for(auto& cStub: cEvent->StubVector(cFeId, static_cast<ReadoutChip*>(cCbcVector.at(iCBC))->getChipId()))
                                                    {
                                                        if(((seedChan[smg] - 1) / 2) + (iCBC * 127) == (cStub.getCenter() + (iCBC * 127)))
                                                        {
                                                            stubfinder = true;
                                                            if(abs((bend / 2) - Decoding_stub4(cStub.getBend())) > 0.25)
                                                            {
                                                                LOG(INFO) << RED << "Wrong stub bend at " << +(((seedChan[smg] - 1) / 2) + (iCBC * 127)) << " | Expected: " << +(bend / 2)
                                                                          << ", measured: " << +Decoding_stub4(cStub.getBend()) << RESET;
                                                                hSTUB_SCAN_error->Fill(((seedChan[smg] - 1) / 2) + (iCBC * 127), (bend / 2));
                                                            }
                                                            break;
                                                        }
                                                    }
                                                    if(!stubfinder)
                                                    {
                                                        LOG(INFO) << RED << "Missing CBC HIT at " << +(((seedChan[smg] - 1) / 2) + (iCBC * 127)) << RESET;
                                                        hSTUB_SCAN_error->Fill(((seedChan[smg] - 1) / 2) + (iCBC * 127), (bend / 2));
                                                    }
                                                }
                                                else
                                                {
                                                    LOG(INFO) << RED << "NO CBC DATA at " << +(((seedChan[smg] - 1) / 2) + (iCBC * 127)) << RESET;
                                                    hSTUB_SCAN_error->Fill(((seedChan[smg] - 1) / 2) + (iCBC * 127), (bend / 2));
                                                }
                                            }
                                            if(cEvent->StubBit(cFeId, static_cast<ReadoutChip*>(cCbcVector.at(iCBC))->getChipId()))
                                            {
                                                uint8_t stubCounter = 0;
                                                for(auto& cStub: cEvent->StubVector(cFeId, static_cast<ReadoutChip*>(cCbcVector.at(iCBC))->getChipId()))
                                                {
                                                    stubCounter++;
                                                    double stub_position = cStub.getPosition();
                                                    double stub_bend     = Decoding_stub4(cStub.getBend());
                                                    double stub_strip    = cStub.getCenter();
                                                    LOG(DEBUG) << "CBC" << +iCBC << " , Stub: " << +(stubCounter) << " | Position: " << stub_position
                                                               << " , EXPECT POS : " << +(seedChan[stubCounter - 1]) << " | Bend: " << std::bitset<4>(cStub.getBend()) << " -> " << stub_bend
                                                               << " , EXPECT BEND: " << +(bend / 2) << " || Strip: " << stub_strip
                                                               << " , EXPECTED STRIP : " << (((seedChan[stubCounter - 1] - 1) / 2) + (iCBC * 127))
                                                               << " , Filling STRIP: " << +(stub_strip + (iCBC * 127));
                                                    hSTUB_SCAN_bend_off[iCBC]->Fill(bend / 2, stub_bend);
                                                    hSTUB_SCAN_bend->Fill(stub_strip + (iCBC * 127), stub_bend);
                                                    fCanvas6->cd();
                                                    hSTUB_SCAN_bend->Draw("COLZ");
                                                    hSTUB_SCAN_bend->SetStats(0);
                                                    fCanvas6->Update();

                                                    if(bend != 0) continue;
                                                    hSTUB_SCAN_tg->Fill(stub_strip + (iCBC * 127), tg);
                                                    fCanvas5->cd();
                                                    hSTUB_SCAN_tg->Draw("COLZ");
                                                    hSTUB_SCAN_tg->SetStats(0);
                                                    fCanvas5->Update();
                                                } // end of stub loop
                                            }     // enf of stub condition
                                            else
                                                LOG(DEBUG) << RED << "!!!!! NO STUB in CBC" << +iCBC << " | Event : " << +countEvent << " !!!!!!!!!" << RESET;
                                        } // end of CBC loop
                                    }     // end event loop
                                }         // end of skip 16 channels loop
                            }             // end of step of 96 channels loop
                        }                 // end of channel loop
                    }                     // end bend loop
                }                         // end of TG loop
                hSTUB_SCAN_tg->Write();
                hSTUB_SCAN_bend->Write();
                hSTUB_SCAN_error->Write();
                for(uint8_t iCBC = 0; iCBC < nCBC; iCBC++) { hSTUB_SCAN_bend_off[iCBC]->Write(); }
            }
        }
    }
}

void StubTool::scanStubs_clusterWidth(unsigned int teststrip)
{
    std::stringstream outp;
    LOG(DEBUG) << GREEN << "Testing Strip " << +teststrip << RESET;
    if(teststrip > 127)
    {
        LOG(INFO) << RED << "Strip range values are 0 to 127" << RESET;
        exit(EXIT_FAILURE);
    }
    for(auto cBoard: *fDetectorContainer)
    {
        BeBoard* theBoard = static_cast<BeBoard*>(cBoard);
        for(auto cOpticalGroup: *cBoard)
        {
            for(auto cFe: *cOpticalGroup)
            {
                uint32_t         cFeId            = cFe->getId();
                ModuleContainer& cCbcVector       = *cFe;
                const uint8_t    nCBC             = cCbcVector.size();
                std::string      stubscanname_cw  = "StubsSCAN_ClusterWidth";
                std::string      stubscanname_cbc = "StubsSCAN_ClusterWidth_vs_Strips";
                // Uncoment for Bend uncoding 2
                // hSTUB_SCAN_tg = new TH2F(stubscanname_tg.c_str(),stubscanname_tg.c_str(),nChan,0,nChan,16,0,8);
                // hSTUB_SCAN_bend = new
                // TH2F(stubscanname_bend.c_str(),stubscanname_bend.c_str(),nChan,0,nChan,16,-6.75,8.25); Comment for
                // Bend uncoding 4
                hSTUB_SCAN_cw  = new TH2F(stubscanname_cw.c_str(), stubscanname_cw.c_str(), 4, 0.5, 4.5, 5, -0.5, 4.5);
                hSTUB_SCAN_cbc = new TH2F(stubscanname_cbc.c_str(), stubscanname_cbc.c_str(), nCBC * 127, 0, nCBC * 127, 4, 0.5, 4.5);
                std::vector<std::string> vec_stubscanname_cw(nCBC, "");
                for(uint8_t iCBC = 0; iCBC < nCBC; iCBC++)
                {
                    vec_stubscanname_cw[iCBC] = "StubsSCAN_ClusterWidth_CBC" + std::to_string(iCBC);
                    hSTUB_SCAN_cw_cbc[iCBC]   = new TH2F(vec_stubscanname_cw[iCBC].c_str(), vec_stubscanname_cw[iCBC].c_str(), 4, 0.5, 4.5, 5, -0.5, 4.5);
                    // hSTUB_SCAN_bend_off[iCBC] = new
                    // TH2F(vec_stubscanname_bend_offset[iCBC].c_str(),vec_stubscanname_bend_offset[iCBC].c_str(),31,-7.5,7.5,16,-6.75,8.25);
                }

                setInitialOffsets();

                // set the threshold, correlation window (pt), bend decoding in all chips!
                uint16_t         cVcth = 700;
                ThresholdVisitor cThresholdVisitor(fReadoutChipInterface, cVcth);
                this->accept(cThresholdVisitor);
                unsigned int Pipe_StubSel_Ptwidth = 14;
                unsigned int BendReg[]            = {153, 170, 187, 204, 221, 238, 255, 16, 33, 50, 67, 84, 101, 118, 135};
                // unsigned int BendReg[] = {153, 170, 187, 204, 221, 238, 255, 16, 33, 50, 67, 84, 101, 118, 120};
                std::vector<std::pair<std::string, uint16_t>> cRegVec;
                uint16_t                                      cRegValue;
                std::string                                   cRegName;
                // double exp_strip = -1;

                for(uint8_t regClusterWidth = 0; regClusterWidth <= 4; ++regClusterWidth)
                {
                    for(uint8_t iClusterWidth = 1; iClusterWidth <= 4; ++iClusterWidth)
                    {
                        // if (regClusterWidth != 0) continue;
                        for(uint8_t iCBC = 0; iCBC < nCBC; iCBC++)
                        {
                            cRegVec.push_back(std::make_pair("Pipe&StubInpSel&Ptwidth", Pipe_StubSel_Ptwidth));
                            cRegVec.push_back(std::make_pair("LayerSwap&CluWidth", regClusterWidth));
                            for(int ireg = 0; ireg < 15; ireg++) { cRegVec.push_back(std::make_pair("Bend" + std::to_string(ireg), BendReg[ireg])); }
                            // fReadoutChipInterface->WriteChipMultReg (static_cast<ReadoutChip*>(cCbcVector.at(iCBC)),
                            // cRegVec ); cRegVec.clear(); MASKING ALL CHANNELS
                            for(unsigned int i = 0; i < fChannelMaskMapCBC3.size(); i++)
                            {
                                static_cast<ReadoutChip*>(cCbcVector.at(iCBC))->setReg(fChannelMaskMapCBC3[i], 0);
                                cRegValue = static_cast<ReadoutChip*>(cCbcVector.at(iCBC))->getReg(fChannelMaskMapCBC3[i]);
                                cRegName  = fChannelMaskMapCBC3[i];
                                cRegVec.push_back(std::make_pair(cRegName, cRegValue));
                                // LOG (DEBUG) << fChannelMaskMapCBC3[i] << " " << std::bitset<8> (cRegValue);
                            }
                            fReadoutChipInterface->WriteChipMultReg(static_cast<ReadoutChip*>(cCbcVector.at(iCBC)), cRegVec);
                            cRegVec.clear();
                            if(teststrip + iClusterWidth > 127)
                            {
                                for(uint8_t iChan = 0; iChan < (iClusterWidth * 2); ++iChan)
                                {
                                    LOG(DEBUG) << "CBC" << +iCBC << " | RegClusterWidth = " << +regClusterWidth << " - Cluster Width = " << +iClusterWidth << " | Diff Chan = " << -iChan
                                               << " | Cond2: masking channel " << +((2 * teststrip) - iChan);
                                    maskChannel(static_cast<ReadoutChip*>(cCbcVector.at(iCBC)), ((2 * teststrip) - iChan), false);
                                    // exp_strip = teststrip-(iClusterWidth/2);
                                }
                            }
                            else
                            {
                                for(uint8_t iChan = 1; iChan <= (iClusterWidth * 2); ++iChan)
                                {
                                    LOG(DEBUG) << "CBC" << +iCBC << " | RegClusterWidth = " << +regClusterWidth << " - Cluster Width = " << +iClusterWidth << " | Diff Chan = " << +iChan
                                               << " | Cond1: masking channel " << +((2 * teststrip) + iChan);
                                    maskChannel(static_cast<ReadoutChip*>(cCbcVector.at(iCBC)), ((2 * teststrip) + iChan), false);
                                    // exp_strip = teststrip+(iClusterWidth/2);
                                }
                            }

                            // CHECKING CBC REGISTERS
                            // CheckCbcReg(static_cast<ReadoutChip*>(cCbcVector.at(iCBC)));
                        }

                        // now read Events
                        ReadNEvents(theBoard, fNevents);
                        const std::vector<Event*> cEvents    = GetEvents(theBoard);
                        int                       countEvent = 0;
                        for(auto cEvent: cEvents)
                        {
                            ++countEvent;
                            for(uint8_t iCBC = 0; iCBC < nCBC; iCBC++)
                            {
                                std::vector<uint32_t> cHits = cEvent->GetHits(cFeId, static_cast<ReadoutChip*>(cCbcVector.at(iCBC))->getChipId());
                                if(cHits.size() == nChan)
                                    LOG(DEBUG) << RED << "CBC " << +iCBC << ": All channels firing!" << RESET;
                                else
                                {
                                    LOG(DEBUG) << BLUE << "List of hits CBC" << +iCBC << ": " << RESET;
                                    for(uint32_t cHit: cHits)
                                    {
                                        double HIT   = cHit + 1;
                                        double STRIP = floor((HIT - 1) / 2);
                                        LOG(DEBUG) << BLUE << std::dec << cHit << " : " << HIT << " , " << STRIP << RESET;
                                    }
                                }
                                if(cEvent->StubBit(cFeId, static_cast<ReadoutChip*>(cCbcVector.at(iCBC))->getChipId()))
                                {
                                    uint8_t stubCounter = 0;
                                    for(auto& cStub: cEvent->StubVector(cFeId, static_cast<ReadoutChip*>(cCbcVector.at(iCBC))->getChipId()))
                                    {
                                        stubCounter++;
                                        double stub_strip = cStub.getCenter();
                                        // if ( !((teststrip+iClusterWidth>127 && stubCounter == nstub) ||
                                        // (teststrip+iClusterWidth<=127 && stubCounter == 1)) ) continue; LOG (DEBUG) <<
                                        // RED << "CBC" << +iCBC << " , Stub: " << +(stubCounter) << " | Position: " <<
                                        // stub_position << " | Bend: " << std::bitset<4> (cStub.getBend()) << " -> " <<
                                        // stub_bend << " || Strip: " << stub_strip << " , EXPECTED STRIP : " <<
                                        // +(exp_strip+(iCBC*127)) << " , Filling STRIP: " << +(stub_strip+(iCBC*127)) <<
                                        // RESET;
                                        hSTUB_SCAN_cw_cbc[iCBC]->Fill(iClusterWidth, regClusterWidth);
                                        hSTUB_SCAN_cw->Fill(iClusterWidth, regClusterWidth);
                                        hSTUB_SCAN_cbc->Fill(stub_strip + (iCBC * 127), iClusterWidth);
                                        fCanvas6->cd();
                                        hSTUB_SCAN_cbc->Draw("COLZ");
                                        hSTUB_SCAN_cbc->SetStats(0);
                                        fCanvas6->Update();

                                    } // end of stub loop
                                }     // enf of stub condition
                                else
                                    LOG(DEBUG) << RED << "!! NO STUB in CBC" << +iCBC << " , ClusterWidthReg = " << +regClusterWidth << " - ClusterWidth = " << +iClusterWidth
                                               << " | Event : " << +countEvent << " !!" << RESET;
                            } // end of CBC loop
                        }     // end event loop
                    }         // end cluster width loop
                }             // end cluster width register loop
                hSTUB_SCAN_cw->Write();
                hSTUB_SCAN_cbc->Write();
                for(uint8_t iCBC = 0; iCBC < nCBC; iCBC++) { hSTUB_SCAN_cw_cbc[iCBC]->Write(); }
            }
        }
    }
}

void StubTool::scanStubs_ptWidth()
{
    std::stringstream outp;
    for(auto cBoard: *fDetectorContainer)
    {
        BeBoard* theBoard = static_cast<BeBoard*>(cBoard);
        for(auto cOpticalGroup: *cBoard)
        {
            for(auto cFe: *cOpticalGroup)
            {
                uint32_t         cFeId      = cFe->getId();
                ModuleContainer& cCbcVector = *cFe;
                const uint8_t    nCBC       = cCbcVector.size();
                // Uncoment for Bend uncoding 2
                // hSTUB_SCAN_tg = new TH2F(stubscanname_tg.c_str(),stubscanname_tg.c_str(),nChan,0,nChan,16,0,8);
                // hSTUB_SCAN_bend = new
                // TH2F(stubscanname_bend.c_str(),stubscanname_bend.c_str(),nChan,0,nChan,16,-6.75,8.25); Comment for
                // Bend uncoding 4
                double      actualbend         = 7;
                double      binbend4[]         = {-7.25, -6.25, -5.25, -4.25, -3.25, -2.25, -1.25, -0.25, 0.25, 1.25, 2.25, 3.25, 4.25, 5.25, 6.25, 7.25, 8.25};
                std::string stubscanname_tg    = "StubsSCAN_TG_CBC";
                std::string stubscanname_bend  = "StubsSCAN_BEND_CBC";
                std::string stubscanname_error = "StubsSCAN_BEND_ERRORS";
                hSTUB_SCAN_tg                  = new TH2F(stubscanname_tg.c_str(), stubscanname_tg.c_str(), nCBC * 127, 0, nCBC * 127, 16, 0, 8);
                hSTUB_SCAN_bend                = new TH2F(stubscanname_bend.c_str(), stubscanname_bend.c_str(), nCBC * 127, 0, nCBC * 127, 16, binbend4);
                hSTUB_SCAN_error               = new TH2F(stubscanname_error.c_str(), stubscanname_error.c_str(), nCBC * 127, 0, nCBC * 127, 16, binbend4);
                std::vector<std::string> vec_stubscanname_bend_offset(nCBC, "");
                std::vector<std::string> vec_stubscanname_pt(nCBC, "");
                for(uint8_t iCBC = 0; iCBC < nCBC; iCBC++)
                {
                    vec_stubscanname_bend_offset[iCBC] = "StubsSCAN_BEND_OFF_CBC" + std::to_string(iCBC);
                    vec_stubscanname_pt[iCBC]          = "StubsSCAN_ptWidth_CBC" + std::to_string(iCBC);
                    hSTUB_SCAN_bend_off[iCBC]          = new TH2F(vec_stubscanname_bend_offset[iCBC].c_str(), vec_stubscanname_bend_offset[iCBC].c_str(), 31, -8, 8, 16, binbend4);
                    hSTUB_SCAN_bend_pt[iCBC]           = new TH2F(vec_stubscanname_pt[iCBC].c_str(), vec_stubscanname_pt[iCBC].c_str(), 7, 0.5, 7.5, 16, binbend4);
                }
                for(uint8_t iptWidth = 0; iptWidth < 7; iptWidth++)
                {
                    for(uint8_t tg = 0; tg < 8; tg++)
                    {
                        LOG(DEBUG) << GREEN << "Test Group " << +tg << RESET;

                        setInitialOffsets();
                        // get channels in test group
                        std::vector<uint8_t> cChannelVector;
                        cChannelVector.clear();
                        cChannelVector = findChannelsInTestGroup(tg);

                        // set the threshold, bend decoding in all chips!
                        uint16_t         cVcth = 700;
                        ThresholdVisitor cThresholdVisitor(fReadoutChipInterface, cVcth);
                        this->accept(cThresholdVisitor);
                        unsigned int Pipe_StubSel_Ptwidth = (iptWidth + 1) * 2;
                        unsigned int BendReg[]            = {153, 170, 187, 204, 221, 238, 255, 16, 33, 50, 67, 84, 101, 118, 135};
                        // unsigned int BendReg[] = {153, 170, 187, 204, 221, 238, 255, 16, 33, 50, 67, 84, 101, 118,
                        // 120};
                        std::vector<std::pair<std::string, uint16_t>> cRegVec;
                        for(uint8_t iCBC = 0; iCBC < nCBC; iCBC++)
                        {
                            cRegVec.push_back(std::make_pair("Pipe&StubInpSel&Ptwidth", Pipe_StubSel_Ptwidth));
                            for(int ireg = 0; ireg < 15; ireg++) { cRegVec.push_back(std::make_pair("Bend" + std::to_string(ireg), BendReg[ireg])); }
                            fReadoutChipInterface->WriteChipMultReg(static_cast<ReadoutChip*>(cCbcVector.at(iCBC)), cRegVec);
                            cRegVec.clear();
                        }

                        for(double bend = -(actualbend * 2); bend <= (actualbend * 2); bend += 1)
                        { // step of halves: 2 halves = 1 strip
                            uint8_t     cRegValue;
                            std::string cRegName;
                            // Unmasking desired channels
                            for(unsigned int iChan = 0; iChan < nChan; iChan++)
                            {
                                if(iChan % 96 != 0) continue;
                                {
                                    for(uint8_t step = 0; step <= 1; step++)
                                    {
                                        for(uint8_t iCBC = 0; iCBC < nCBC; iCBC++)
                                        {
                                            // MASKING ALL CHANNELS

                                            for(unsigned int i = 0; i < fChannelMaskMapCBC3.size(); i++)
                                            {
                                                static_cast<ReadoutChip*>(cCbcVector.at(iCBC))->setReg(fChannelMaskMapCBC3[i], 0);
                                                cRegValue = static_cast<ReadoutChip*>(cCbcVector.at(iCBC))->getReg(fChannelMaskMapCBC3[i]);
                                                cRegName  = fChannelMaskMapCBC3[i];
                                                cRegVec.push_back(std::make_pair(cRegName, cRegValue));
                                                // LOG (DEBUG) << fChannelMaskMapCBC3[i] << " " << std::bitset<8>
                                                // (cRegValue);
                                            }
                                            fReadoutChipInterface->WriteChipMultReg(static_cast<ReadoutChip*>(cCbcVector.at(iCBC)), cRegVec);
                                            cRegVec.clear();
                                        }

                                        int seedChan[3] = {-1};
                                        LOG(DEBUG) << "--------------";
                                        for(unsigned int smg = 0; smg < 3; smg++)
                                        {
                                            seedChan[smg] = (iChan + (smg * 32) + (step * 16) + (tg * 2) + 1);
                                            if(seedChan[smg] < 0 || seedChan[smg] > nChan) continue;
                                            for(uint8_t iCBC = 0; iCBC < nCBC; iCBC++)
                                            {
                                                if((seedChan[smg] + bend - 1) < 0)
                                                {
                                                    if(iCBC == 0) continue;
                                                    LOG(DEBUG) << "Channel (CBC" << +(iCBC) << "): " << +seedChan[smg];
                                                    int corrChan = nChan + (seedChan[smg] + bend + 1);
                                                    if((int)bend % 2 != 0) corrChan = nChan + (seedChan[smg] + bend);
                                                    if(corrChan % 2 != 0 || corrChan <= 0) continue;
                                                    maskChannel(static_cast<ReadoutChip*>(cCbcVector.at(iCBC - 1)), corrChan, false);
                                                    maskChannel(static_cast<ReadoutChip*>(cCbcVector.at(iCBC)), seedChan[smg], false);
                                                    LOG(DEBUG) << "Cond 1: Corr Chan (CBC" << +(iCBC - 1) << "): " << +corrChan << " (" << +(seedChan[smg] + bend - 1) << ")";
                                                    if((int)bend % 2 != 0 && corrChan + 2 > 0)
                                                    {
                                                        maskChannel(static_cast<ReadoutChip*>(cCbcVector.at(iCBC - 1)), corrChan + 2, false);
                                                        LOG(DEBUG) << "              , Corr Chan : " << (corrChan + 2);
                                                    }
                                                }
                                                else if((seedChan[smg] + bend + 1) > nChan)
                                                {
                                                    if(iCBC == nCBC - 1) continue;
                                                    LOG(DEBUG) << "Channel (CBC" << +iCBC << "): " << seedChan[smg];
                                                    int corrChan = (seedChan[smg] + bend + 1) - nChan;
                                                    if((int)bend % 2 != 0) corrChan = (seedChan[smg] + bend) - nChan;
                                                    if(corrChan % 2 != 0) continue;
                                                    maskChannel(static_cast<ReadoutChip*>(cCbcVector.at(iCBC)), seedChan[smg], false);
                                                    if(corrChan > 0)
                                                        maskChannel(static_cast<ReadoutChip*>(cCbcVector.at(iCBC + 1)), corrChan, false);
                                                    else if(corrChan == 0)
                                                    {
                                                        corrChan = nChan;
                                                        maskChannel(static_cast<ReadoutChip*>(cCbcVector.at(iCBC)), corrChan, false);
                                                    }
                                                    LOG(DEBUG) << "Cond 2: Corr Chan (" << +(iCBC) << "/" << +(iCBC + 1) << "): " << corrChan << " (" << seedChan[smg] + bend + 1 << ")";
                                                    if(corrChan == nChan)
                                                    {
                                                        corrChan = 2;
                                                        maskChannel(static_cast<ReadoutChip*>(cCbcVector.at(iCBC + 1)), corrChan, false);
                                                        LOG(DEBUG) << "                 , Corr Chan " << +(iCBC + 1) << ": " << (corrChan);
                                                    }
                                                    else if((int)bend % 2 != 0 && corrChan + 2 > 0)
                                                    {
                                                        maskChannel(static_cast<ReadoutChip*>(cCbcVector.at(iCBC + 1)), corrChan + 2, false);
                                                        LOG(DEBUG) << "                 , Corr Chan " << +(iCBC + 1) << ": " << (corrChan + 2);
                                                    }
                                                }
                                                else
                                                {
                                                    LOG(DEBUG) << "Channel (CBC" << +iCBC << "): " << seedChan[smg];
                                                    int corrChan = (seedChan[smg] + bend + 1);
                                                    if((int)bend % 2 != 0) corrChan = (seedChan[smg] + bend);
                                                    if(corrChan % 2 != 0 || corrChan <= 0) continue;
                                                    maskChannel(static_cast<ReadoutChip*>(cCbcVector.at(iCBC)), seedChan[smg], false);
                                                    maskChannel(static_cast<ReadoutChip*>(cCbcVector.at(iCBC)), corrChan, false);
                                                    LOG(DEBUG) << "Cond 3: Corr Chan (CBC" << +(iCBC) << "): " << corrChan << " (" << seedChan[smg] + bend + 1 << ")";
                                                    if((int)bend % 2 != 0 && corrChan + 2 > 0)
                                                    {
                                                        maskChannel(static_cast<ReadoutChip*>(cCbcVector.at(iCBC)), corrChan + 2, false);
                                                        LOG(DEBUG) << "         , Corr Chan : " << (corrChan + 2);
                                                    }
                                                }
                                                // MASKING BAD CHANNELS FOR CBC3.0
                                                /*if (iCBC > 0)
                                                {
                                                  maskChannel(static_cast<ReadoutChip*>(cCbcVector.at(iCBC-1)), 107,
                                                true); maskChannel(static_cast<ReadoutChip*>(cCbcVector.at(iCBC-1)),
                                                225, true);
                                                }
                                                maskChannel(static_cast<ReadoutChip*>(cCbcVector.at(iCBC)), 107, true);
                                                maskChannel(static_cast<ReadoutChip*>(cCbcVector.at(iCBC)), 225, true);
                                                */
                                            }
                                        }

                                        // CHECKING CBC REGISTERS
                                        // for (uint8_t iCBC = 0; i< nCBC; iCBC++)
                                        // {CheckCbcReg(static_cast<ReadoutChip*>(cCbcVector.at(iCBC)))};

                                        // now read Events
                                        ReadNEvents(theBoard, fNevents);
                                        const std::vector<Event*> cEvents    = GetEvents(theBoard);
                                        int                       countEvent = 0;
                                        for(auto cEvent: cEvents)
                                        {
                                            ++countEvent;
                                            for(uint8_t iCBC = 0; iCBC < nCBC; iCBC++)
                                            {
                                                std::vector<uint32_t> cHits = cEvent->GetHits(cFeId, static_cast<ReadoutChip*>(cCbcVector.at(iCBC))->getChipId());
                                                if(cHits.size() == nChan)
                                                    LOG(INFO) << RED << "CBC " << +iCBC << ": All channels firing!" << RESET;
                                                else
                                                {
                                                    LOG(DEBUG) << BLUE << "List of hits CBC" << +iCBC << ": " << RESET;
                                                    for(uint32_t cHit: cHits)
                                                    {
                                                        double HIT   = cHit + 1;
                                                        double STRIP = floor((HIT - 1) / 2);
                                                        LOG(DEBUG) << BLUE << std::dec << cHit << " : " << HIT << " , " << STRIP << RESET;
                                                        if(bend == 0 && iptWidth == 6 && (int)HIT % 2 != 0) hSTUB_SCAN_tg->Fill(STRIP + (iCBC * 127), tg + 0.5, 0.5);
                                                    }
                                                }
                                                if(iptWidth == 6)
                                                    for(uint8_t smg = 0; smg < 3; smg++)
                                                    {
                                                        if(seedChan[smg] < 0 || seedChan[smg] > nChan) continue;
                                                        if(((seedChan[smg] - 1) / 2) + (iCBC * 127) + (bend / 2) < 0 || ((seedChan[smg] + 1) / 2) + (iCBC * 127) + (bend / 2) > nCBC * 127) continue;
                                                        if(cEvent->StubBit(cFeId, static_cast<ReadoutChip*>(cCbcVector.at(iCBC))->getChipId()))
                                                        {
                                                            bool stubfinder = false;
                                                            for(auto& cStub: cEvent->StubVector(cFeId, static_cast<ReadoutChip*>(cCbcVector.at(iCBC))->getChipId()))
                                                            {
                                                                if(((seedChan[smg] - 1) / 2) + (iCBC * 127) == (cStub.getCenter() + (iCBC * 127)))
                                                                {
                                                                    stubfinder = true;
                                                                    if(abs((bend / 2) - Decoding_stub4(cStub.getBend())) > 0.25)
                                                                    {
                                                                        LOG(INFO) << RED << "Wrong stub bend at " << +(((seedChan[smg] - 1) / 2) + (iCBC * 127)) << " | Expected: " << +(bend / 2)
                                                                                  << ", measured: " << +Decoding_stub4(cStub.getBend()) << RESET;
                                                                        hSTUB_SCAN_error->Fill(((seedChan[smg] - 1) / 2) + (iCBC * 127), (bend / 2));
                                                                    }
                                                                    break;
                                                                }
                                                            }
                                                            if(!stubfinder)
                                                            {
                                                                LOG(INFO) << RED << "Missing CBC HIT at " << +(((seedChan[smg] - 1) / 2) + (iCBC * 127)) << RESET;
                                                                hSTUB_SCAN_error->Fill(((seedChan[smg] - 1) / 2) + (iCBC * 127), (bend / 2));
                                                            }
                                                        }
                                                        else
                                                        {
                                                            LOG(INFO) << RED << "NO CBC DATA at " << +(((seedChan[smg] - 1) / 2) + (iCBC * 127)) << RESET;
                                                            hSTUB_SCAN_error->Fill(((seedChan[smg] - 1) / 2) + (iCBC * 127), (bend / 2));
                                                        }
                                                    }
                                                if(cEvent->StubBit(cFeId, static_cast<ReadoutChip*>(cCbcVector.at(iCBC))->getChipId()))
                                                {
                                                    uint8_t stubCounter = 1;
                                                    for(auto& cStub: cEvent->StubVector(cFeId, static_cast<ReadoutChip*>(cCbcVector.at(iCBC))->getChipId()))
                                                    {
                                                        double stub_position = cStub.getPosition();
                                                        double stub_bend     = Decoding_stub4(cStub.getBend());
                                                        double stub_strip    = cStub.getCenter();
                                                        LOG(DEBUG) << "CBC" << +iCBC << " - ptWidht: " << +(iptWidth + 1) << " , Stub: " << +stubCounter << " | Position: " << stub_position
                                                                   << " , EXPECT POS : " << +(seedChan[stubCounter - 1] + 1) << " | Bend: " << std::bitset<4>(cStub.getBend()) << " -> " << stub_bend
                                                                   << " , EXPECT BEND: " << +(bend / 2) << " || Strip: " << stub_strip
                                                                   << " , EXPECTED STRIP : " << (((seedChan[stubCounter - 1] - 1) / 2) + (iCBC * 127))
                                                                   << " , Filling STRIP: " << +(stub_strip + (iCBC * 127));
                                                        stubCounter++;
                                                        hSTUB_SCAN_bend_pt[iCBC]->Fill(iptWidth + 1, stub_bend);
                                                        if(iptWidth != 6) continue;
                                                        hSTUB_SCAN_bend_off[iCBC]->Fill(bend / 2, stub_bend);
                                                        hSTUB_SCAN_bend->Fill(stub_strip + (iCBC * 127), stub_bend);
                                                        fCanvas6->cd();
                                                        hSTUB_SCAN_bend->Draw("COLZ");
                                                        hSTUB_SCAN_bend->SetStats(0);
                                                        fCanvas6->Update();

                                                        if(bend != 0) continue;
                                                        hSTUB_SCAN_tg->Fill(stub_strip + (iCBC * 127), tg);
                                                        fCanvas5->cd();
                                                        hSTUB_SCAN_tg->Draw("COLZ");
                                                        hSTUB_SCAN_tg->SetStats(0);
                                                        fCanvas5->Update();
                                                    } // end of stub loop
                                                }     // enf of stub condition
                                                else
                                                    LOG(DEBUG) << RED << "!!!!! NO STUB in CBC" << +iCBC << " | Event : " << +countEvent << " !!!!!!!!!" << RESET;
                                            } // end of CBC loop
                                        }     // end event loop
                                    }         // end of skip 16 channels loop
                                }             // end of step of 96 channels loop
                            }                 // end of channel loop
                        }                     // end bend loop
                    }                         // end of TG loop
                }
                hSTUB_SCAN_tg->Write();
                hSTUB_SCAN_bend->Write();
                hSTUB_SCAN_error->Write();
                for(uint8_t iCBC = 0; iCBC < nCBC; iCBC++)
                {
                    hSTUB_SCAN_bend_pt[iCBC]->Write();
                    hSTUB_SCAN_bend_off[iCBC]->Write();
                }
            }
        }
    }
}

void StubTool::scanStubs_SoF(unsigned int teststrip)
{
    std::stringstream outp;
    LOG(DEBUG) << GREEN << "Testing Strip " << +teststrip << RESET;
    if(teststrip > 127)
    {
        LOG(INFO) << RED << "Strip range values are 0 to 127" << RESET;
        exit(EXIT_FAILURE);
    }
    for(auto cBoard: *fDetectorContainer)
    {
        BeBoard* theBoard = static_cast<BeBoard*>(cBoard);
        for(auto cOpticalGroup: *cBoard)
        {
            for(auto cFe: *cOpticalGroup)
            {
                uint32_t         cFeId            = cFe->getId();
                ModuleContainer& cCbcVector       = *cFe;
                const uint8_t    nCBC             = cCbcVector.size();
                std::string      stubscanname_sof = "StubsSCAN_SoF";
                std::string      stubscanname_cbc = "StubsSCAN_SoF_vs_Strips";
                // Uncoment for Bend uncoding 2
                // hSTUB_SCAN_tg = new TH2F(stubscanname_tg.c_str(),stubscanname_tg.c_str(),nChan,0,nChan,16,0,8);
                // hSTUB_SCAN_bend = new
                // TH2F(stubscanname_bend.c_str(),stubscanname_bend.c_str(),nChan,0,nChan,16,-6.75,8.25); Comment for
                // Bend uncoding 4
                hSTUB_SCAN_sof = new TH2F(stubscanname_sof.c_str(), stubscanname_sof.c_str(), 5, 0.5, 5.5, 2, -0.5, 1.5);
                hSTUB_SCAN_SOF = new TH2F(stubscanname_cbc.c_str(), stubscanname_cbc.c_str(), nCBC * 127, 0, nCBC * 127, 2, -0.5, 1.5);
                std::vector<std::string> vec_stubscanname_sof(nCBC, "");
                for(uint8_t iCBC = 0; iCBC < nCBC; iCBC++)
                {
                    vec_stubscanname_sof[iCBC] = "StubsSCAN_ClusterWidth_CBC" + std::to_string(iCBC);
                    hSTUB_SCAN_sof_cbc[iCBC]   = new TH2F(vec_stubscanname_sof[iCBC].c_str(), vec_stubscanname_sof[iCBC].c_str(), 5, 0.5, 5.5, 2, -0.5, 1.5);
                    // hSTUB_SCAN_bend_off[iCBC] = new
                    // TH2F(vec_stubscanname_bend_offset[iCBC].c_str(),vec_stubscanname_bend_offset[iCBC].c_str(),31,-7.5,7.5,16,-6.75,8.25);
                }

                setInitialOffsets();

                // set the threshold, correlation window (pt), bend decoding in all chips!
                uint16_t         cVcth = 700;
                ThresholdVisitor cThresholdVisitor(fReadoutChipInterface, cVcth);
                this->accept(cThresholdVisitor);
                unsigned int Pipe_StubSel_Ptwidth = 14;
                unsigned int BendReg[]            = {153, 170, 187, 204, 221, 238, 255, 16, 33, 50, 67, 84, 101, 118, 135};
                // unsigned int BendReg[] = {153, 170, 187, 204, 221, 238, 255, 16, 33, 50, 67, 84, 101, 118, 120};
                std::vector<std::pair<std::string, uint16_t>> cRegVec;
                uint16_t                                      cRegValue;
                std::string                                   cRegName;

                for(uint8_t nstubs = 1; nstubs <= 5; ++nstubs)
                {
                    // if (regClusterWidth != 0) continue;
                    for(uint8_t iCBC = 0; iCBC < nCBC; iCBC++)
                    {
                        cRegVec.push_back(std::make_pair("Pipe&StubInpSel&Ptwidth", Pipe_StubSel_Ptwidth));
                        for(int ireg = 0; ireg < 15; ireg++) { cRegVec.push_back(std::make_pair("Bend" + std::to_string(ireg), BendReg[ireg])); }

                        // MASKING ALL CHANNELS
                        for(unsigned int i = 0; i < fChannelMaskMapCBC3.size(); i++)
                        {
                            static_cast<ReadoutChip*>(cCbcVector.at(iCBC))->setReg(fChannelMaskMapCBC3[i], 0);
                            cRegValue = static_cast<ReadoutChip*>(cCbcVector.at(iCBC))->getReg(fChannelMaskMapCBC3[i]);
                            cRegName  = fChannelMaskMapCBC3[i];
                            cRegVec.push_back(std::make_pair(cRegName, cRegValue));
                            // LOG (DEBUG) << fChannelMaskMapCBC3[i] << " " << std::bitset<8> (cRegValue);
                        }
                        fReadoutChipInterface->WriteChipMultReg(static_cast<ReadoutChip*>(cCbcVector.at(iCBC)), cRegVec);
                        cRegVec.clear();
                        if(teststrip + (8 * 5) > 127)
                        {
                            for(uint8_t istub = 0; istub < nstubs; ++istub)
                            {
                                LOG(DEBUG) << "CBC" << +iCBC << " | nstubs = " << +nstubs << " : Bend = " << +istub << " - Seed ch = " << +(teststrip - istub * 16 + 1);
                                maskChannel(static_cast<ReadoutChip*>(cCbcVector.at(iCBC)), (teststrip - istub * 16 + 1), false);
                                maskChannel(static_cast<ReadoutChip*>(cCbcVector.at(iCBC)), (teststrip - istub * 16 + 2), false);
                            }
                        }
                        else
                        {
                            for(uint8_t istub = 0; istub < nstubs; ++istub)
                            {
                                LOG(DEBUG) << "CBC" << +iCBC << " | nstubs = " << +nstubs << " : Bend = " << +istub << " - Seed ch = " << +(teststrip + istub * 16 + 1);
                                maskChannel(static_cast<ReadoutChip*>(cCbcVector.at(iCBC)), (teststrip + istub * 16 + 1), false);
                                maskChannel(static_cast<ReadoutChip*>(cCbcVector.at(iCBC)), (teststrip + istub * 16 + 2), false);
                            }
                        }

                        // CHECKING CBC REGISTERS
                        // CheckCbcReg(static_cast<ReadoutChip*>(cCbcVector.at(iCBC)));
                    }

                    // now read Events
                    ReadNEvents(theBoard, fNevents);
                    const std::vector<Event*> cEvents    = GetEvents(theBoard);
                    int                       countEvent = 0;
                    for(auto cEvent: cEvents)
                    {
                        ++countEvent;
                        for(uint8_t iCBC = 0; iCBC < nCBC; iCBC++)
                        {
                            std::vector<uint32_t> cHits = cEvent->GetHits(cFeId, static_cast<ReadoutChip*>(cCbcVector.at(iCBC))->getChipId());
                            if(cHits.size() == nChan)
                                LOG(DEBUG) << RED << "CBC " << +iCBC << ": All channels firing!" << RESET;
                            else
                            {
                                LOG(DEBUG) << BLUE << "List of hits CBC" << +iCBC << ": " << RESET;
                                for(uint32_t cHit: cHits)
                                {
                                    double HIT   = cHit + 1;
                                    double STRIP = floor((HIT - 1) / 2);
                                    LOG(DEBUG) << BLUE << std::dec << cHit << " : " << HIT << " , " << STRIP << RESET;
                                }
                            }
                            if(cEvent->StubBit(cFeId, static_cast<ReadoutChip*>(cCbcVector.at(iCBC))->getChipId()))
                            {
                                uint8_t stubCounter = 0;
                                uint8_t nstub       = cEvent->StubVector(cFeId, static_cast<ReadoutChip*>(cCbcVector.at(iCBC))->getChipId()).size();
                                for(auto& cStub: cEvent->StubVector(cFeId, static_cast<ReadoutChip*>(cCbcVector.at(iCBC))->getChipId()))
                                {
                                    stubCounter++;
                                    double stub_position = cStub.getPosition();
                                    double stub_bend     = Decoding_stub4(cStub.getBend());
                                    double stub_strip    = cStub.getCenter();
                                    LOG(DEBUG) << RED << "CBC" << +iCBC << " , Stub: " << +(stubCounter) << " | Position: " << stub_position << " | Bend: " << std::bitset<4>(cStub.getBend()) << " -> "
                                               << stub_bend << " || Strip: " << stub_strip << " , Filling STRIP: " << +(stub_strip + (iCBC * 127)) << RESET;
                                    uint16_t                     cKey  = encodeId(cFeId, static_cast<ReadoutChip*>(cCbcVector.at(iCBC))->getChipId());
                                    EventDataMap::const_iterator cData = cEvent->fEventDataMap.find(cKey);
                                    if(cData != std::end(cEvent->fEventDataMap))
                                    {
                                        uint8_t pos1  = (cData->second.at(13) & 0x000000FF);
                                        uint8_t pos2  = (cData->second.at(13) & 0x0000FF00) >> 8;
                                        uint8_t pos3  = (cData->second.at(13) & 0x00FF0000) >> 16;
                                        uint8_t bend1 = (cData->second.at(14) & 0x00000F00) >> 8;
                                        uint8_t bend2 = (cData->second.at(14) & 0x000F0000) >> 16;
                                        uint8_t bend3 = (cData->second.at(14) & 0x0F000000) >> 24;
                                        //-----------
                                        uint8_t sync = (cData->second.at(14) & 0x00000008) >> 3;
                                        //----------
                                        uint8_t sof = (cData->second.at(14) & 0x00000001);
                                        //----------
                                        uint8_t or254 = (cData->second.at(14) & 0x00000002) >> 1;

                                        LOG(DEBUG) << MAGENTA << "DATA Bits part 1: " << std::bitset<32>(cData->second.at(13)) << RESET;
                                        LOG(DEBUG) << MAGENTA << "DATA Bits part 2: " << std::bitset<32>(cData->second.at(14)) << RESET;
                                        LOG(DEBUG) << MAGENTA << "SYNC BIT        : " << std::bitset<32>(0x00000008) << RESET;
                                        LOG(DEBUG) << MAGENTA << "MATCH SYNC      : " << std::bitset<32>((cData->second.at(14) & 0x00000008)) << RESET;
                                        LOG(DEBUG) << MAGENTA << "SOF BIT         : " << std::bitset<32>(0x00000001) << RESET;
                                        LOG(DEBUG) << MAGENTA << "MATCH SOF       : " << std::bitset<32>((cData->second.at(14) & 0x00000001)) << RESET;
                                        LOG(DEBUG) << MAGENTA << "OR254 BIT       : " << std::bitset<32>(0x00000002) << RESET;
                                        LOG(DEBUG) << MAGENTA << "MATCH OR254     : " << std::bitset<32>((cData->second.at(14) & 0x00000002)) << RESET;
                                        LOG(DEBUG) << "POS1:  " << +(pos1);
                                        LOG(DEBUG) << "POS2:  " << +(pos2);
                                        LOG(DEBUG) << "POS3:  " << +(pos3);
                                        LOG(DEBUG) << "BEND1: " << +(bend1);
                                        LOG(DEBUG) << "BEND2: " << +(bend2);
                                        LOG(DEBUG) << "BEND3: " << +(bend3);
                                        LOG(DEBUG) << BOLDBLUE << "Stub Overflow Bit " << +sof << " | SYNC " << +sync << " | OR254 " << +or254 << " , Stubs = " << +nstubs << " / " << +nstub << RESET;

                                        hSTUB_SCAN_sof_cbc[iCBC]->Fill(nstubs, sof);
                                        hSTUB_SCAN_sof->Fill(nstubs, sof);
                                        hSTUB_SCAN_SOF->Fill(stub_strip + (iCBC * 127), sof);
                                        fCanvas6->cd();
                                        hSTUB_SCAN_SOF->Draw("COLZ");
                                        hSTUB_SCAN_SOF->SetStats(0);
                                        fCanvas6->Update();
                                    }
                                }
                            } // enf of stub condition
                            else
                                LOG(DEBUG) << RED << "!! NO STUB in CBC" << +iCBC << " , nstubs = " << +nstubs << " | Event : " << +countEvent << " !!" << RESET;
                        } // end of CBC loop
                    }     // end event loop
                }         // end cluster width register loop
                hSTUB_SCAN_sof->Write();
                hSTUB_SCAN_SOF->Write();
                for(uint8_t iCBC = 0; iCBC < nCBC; iCBC++) { hSTUB_SCAN_sof_cbc[iCBC]->Write(); }
            }
        }
    }
}

void StubTool::setDelayAndTestGroup(uint32_t pDelayns, uint8_t cTestGroup)
{
    // this is a little helper function to vary the Test pulse delay
    // set the fine delay on the CBC (cbc tp delay)
    // set the coarse delay on the D19C

    uint8_t cCoarseDelay = floor(pDelayns / 25);
    uint8_t cFineDelay   = (cCoarseDelay * 25) + 24 - pDelayns;

    // the name of the register controlling the TP timing on D19C
    std::string cTPDelayRegName = "fc7_daq_cnfg.fast_command_block.test_pulse.delay_after_test_pulse";

    fBeBoardInterface->WriteBoardReg(fBoard, cTPDelayRegName, cCoarseDelay);

    CbcRegWriter cWriter(fReadoutChipInterface, "TestPulseDel&ChanGroup", to_reg(cFineDelay, cTestGroup));
    this->accept(cWriter);
}

void StubTool::parseSettings()
{
    // parse the settings
    auto cSetting = fSettingsMap.find("HoleMode");

    if(cSetting != std::end(fSettingsMap))
        fHoleMode = cSetting->second;
    else
        fHoleMode = 1;
    fHoleMode = 0;

    cSetting = fSettingsMap.find("Nevents");

    if(cSetting != std::end(fSettingsMap))
        fNevents = cSetting->second;
    else
        fNevents = 1;
    fNevents = 1;

    cSetting = fSettingsMap.find("TestPulsePotentiometer");

    if(cSetting != std::end(fSettingsMap))
        fTPAmplitude = cSetting->second;
    else
        fTPAmplitude = (fHoleMode) ? 50 : 200;
}

void StubTool::setInitialOffsets()
{
    LOG(INFO) << "Re-applying the original offsets for all CBCs";

    for(auto cBoard: *fDetectorContainer)
    {
        for(auto cOpticalGroup: *cBoard)
        {
            for(auto cFe: *cOpticalGroup)
            {
                for(auto cCbc: *cFe)
                {
                    ReadoutChip*   theCbc = static_cast<ReadoutChip*>(cCbc);
                    RegisterVector cRegVec;

                    for(uint8_t cChan = 0; cChan < nChan; cChan++)
                    {
                        TString cRegName = Form("Channel%03d", cChan + 1);
                        uint8_t cOffset  = theCbc->getReg(cRegName.Data());
                        theCbc->setReg(Form("Channel%03d", cChan + 1), cOffset);
                        cRegVec.push_back({Form("Channel%03d", cChan + 1), cOffset});
                        // LOG (DEBUG) << "Original Offset for CBC " << cCbcId << " channel " << +cChan << " " <<
                        // +cOffset ;
                    }

                    if(theCbc->getFrontEndType() == FrontEndType::CBC3)
                    {
                        // LOG (INFO) << BOLDBLUE << "Chip Type = CBC3 - re-enabling stub logic to original value!" <<
                        // RESET;
                        fStubLogicValue[theCbc] = fReadoutChipInterface->ReadChipReg(theCbc, "Pipe&StubInpSel&Ptwidth");
                        fHIPCountValue[theCbc]  = fReadoutChipInterface->ReadChipReg(theCbc, "HIP&TestMode");
                        cRegVec.push_back({"Pipe&StubInpSel&Ptwidth", fStubLogicValue[theCbc]});
                        cRegVec.push_back({"HIP&TestMode", fHIPCountValue[theCbc]});
                    }

                    fReadoutChipInterface->WriteChipMultReg(theCbc, cRegVec);
                    cRegVec.clear();
                }
            }
        }
    }
}

void StubTool::configureTestPulse(Chip* pCbc, uint8_t pPulseState)
{
    // get value of TestPulse control register
    uint8_t cOrigValue = pCbc->getReg("MiscTestPulseCtrl&AnalogMux");
    uint8_t cRegVal    = cOrigValue | (pPulseState << 6);

    fReadoutChipInterface->WriteChipReg(pCbc, "MiscTestPulseCtrl&AnalogMux", cRegVal);
    cRegVal = pCbc->getReg("MiscTestPulseCtrl&AnalogMux");
    LOG(DEBUG) << "Test pulse register 0x" << std::hex << +cOrigValue << " - " << std::bitset<8>(cOrigValue) << " - now set to: 0x" << std::hex << +cRegVal << " - " << std::bitset<8>(cRegVal);
}

std::vector<uint8_t> StubTool::findChannelsInTestGroup(uint8_t pTestGroup)
{
    std::vector<uint8_t> cChannelVector;

    for(int idx = 0; idx < 16; idx++)
    {
        int ctemp1 = idx * 16 + pTestGroup * 2 + 1;
        int ctemp2 = ctemp1 + 1;

        // added less than or equal to here
        if(ctemp1 <= nChan) cChannelVector.push_back(ctemp1);

        if(ctemp2 <= nChan) cChannelVector.push_back(ctemp2);
    }

    return cChannelVector;
}

void StubTool::setCorrelationWinodwOffsets(Chip* pCbc, double pOffsetR1, double pOffsetR2, double pOffsetR3, double pOffsetR4)
{
    uint8_t cOffsetR1 = fWindowOffsetMapCBC3.find(pOffsetR1)->second;
    uint8_t cOffsetR2 = fWindowOffsetMapCBC3.find(pOffsetR2)->second;
    uint8_t cOffsetR3 = fWindowOffsetMapCBC3.find(pOffsetR3)->second;
    uint8_t cOffsetR4 = fWindowOffsetMapCBC3.find(pOffsetR4)->second;

    uint8_t cOffsetRegR12 = (((cOffsetR2) << 4) | cOffsetR1);
    uint8_t cOffsetRegR34 = (((cOffsetR4) << 4) | cOffsetR3);

    fReadoutChipInterface->WriteChipReg(pCbc, "CoincWind&Offset12", cOffsetRegR12);
    LOG(DEBUG) << "\t"
               << "CoincWind&Offset12" << BOLDBLUE << " set to " << std::bitset<8>(cOffsetRegR12) << " - offsets were supposed to be : " << +cOffsetR1 << " and " << +cOffsetR2 << RESET;

    fReadoutChipInterface->WriteChipReg(pCbc, "CoincWind&Offset34", cOffsetRegR34);
    LOG(DEBUG) << "\t"
               << "CoincWind&Offset34" << BOLDBLUE << " set to " << std::bitset<8>(cOffsetRegR34) << " - offsets were supposed to be : " << +cOffsetR3 << " and " << +cOffsetR4 << RESET;
}

double StubTool::Decoding_stub1(int Stub_pos)
{
    double Bend_map[] = {0, 1, 2, 3.5, 4.75, 5.5, 6, 6.75, 8, -6.75, -6, -5.5, -4.75, -3.5, -2, -1};
    return Bend_map[Stub_pos];
}

double StubTool::Decoding_stub2(int Stub_pos)
{
    double Bend_map[] = {0.25, 1.25, 2.25, 3.25, 4.25, 5.25, 6.25, 8., 7, -6.75, -5.75, -4.75, -3.75, -2.75, -1.75, -0.75};
    return Bend_map[Stub_pos];
}

double StubTool::Decoding_stub3(int Stub_pos)
{
    double Bend_map[] = {0, 0.5, 1., 1.5, 2., 2.5, 3., 3.5, 7.5, 4, 4.5, 5., 5.5, 6., 6.5, 7};
    return Bend_map[Stub_pos];
}

double StubTool::Decoding_stub4(int Stub_pos)
{
    double Bend_map[] = {0, 0.75, 1.75, 2.75, 3.75, 4.75, 5.75, 6.75, 8., -6.75, -5.75, -4.75, -3.75, -2.75, -1.75, -0.75};
    return Bend_map[Stub_pos];
}

void StubTool::CheckCbcReg(Chip* pCbc)
{
    LOG(INFO) << BLUE << "CBC " << pCbc;
    LOG(INFO) << RED << "MiscTestPulseCtrl&AnalogMux " << std::bitset<8>(fReadoutChipInterface->ReadChipReg(pCbc, "MiscTestPulseCtrl&AnalogMux"));
    LOG(INFO) << "TestPulseDel&ChanGroup " << std::bitset<8>(fReadoutChipInterface->ReadChipReg(pCbc, "TestPulseDel&ChanGroup"));
    LOG(INFO) << "VCth1 " << std::bitset<8>(fReadoutChipInterface->ReadChipReg(pCbc, "VCth1"));
    LOG(INFO) << "VCth2 " << std::bitset<8>(fReadoutChipInterface->ReadChipReg(pCbc, "VCth2"));
    LOG(INFO) << "Pipe&StubInpSel&Ptwidth " << std::bitset<8>(fReadoutChipInterface->ReadChipReg(pCbc, "Pipe&StubInpSel&Ptwidth"));
    LOG(INFO) << "LayerSwap&CluWidth " << std::bitset<8>(fReadoutChipInterface->ReadChipReg(pCbc, "LayerSwap&CluWidth"));
    LOG(INFO) << "CoincWind&Offset12 " << std::bitset<8>(fReadoutChipInterface->ReadChipReg(pCbc, "CoincWind&Offset12"));
    LOG(INFO) << "CoincWind&Offset34 " << std::bitset<8>(fReadoutChipInterface->ReadChipReg(pCbc, "CoincWind&Offset34"));
    LOG(INFO) << "Bend0 " << std::bitset<8>(fReadoutChipInterface->ReadChipReg(pCbc, "Bend0"));
    LOG(INFO) << "Bend1 " << std::bitset<8>(fReadoutChipInterface->ReadChipReg(pCbc, "Bend1"));
    LOG(INFO) << "Bend2 " << std::bitset<8>(fReadoutChipInterface->ReadChipReg(pCbc, "Bend2"));
    LOG(INFO) << "Bend3 " << std::bitset<8>(fReadoutChipInterface->ReadChipReg(pCbc, "Bend3"));
    LOG(INFO) << "Bend4 " << std::bitset<8>(fReadoutChipInterface->ReadChipReg(pCbc, "Bend4"));
    LOG(INFO) << "Bend5 " << std::bitset<8>(fReadoutChipInterface->ReadChipReg(pCbc, "Bend5"));
    LOG(INFO) << "Bend6 " << std::bitset<8>(fReadoutChipInterface->ReadChipReg(pCbc, "Bend6"));
    LOG(INFO) << "Bend7 " << std::bitset<8>(fReadoutChipInterface->ReadChipReg(pCbc, "Bend7"));
    LOG(INFO) << "Bend8 " << std::bitset<8>(fReadoutChipInterface->ReadChipReg(pCbc, "Bend8"));
    LOG(INFO) << "Bend9 " << std::bitset<8>(fReadoutChipInterface->ReadChipReg(pCbc, "Bend9"));
    LOG(INFO) << "Bend10 " << std::bitset<8>(fReadoutChipInterface->ReadChipReg(pCbc, "Bend10"));
    LOG(INFO) << "Bend11 " << std::bitset<8>(fReadoutChipInterface->ReadChipReg(pCbc, "Bend11"));
    LOG(INFO) << "Bend12 " << std::bitset<8>(fReadoutChipInterface->ReadChipReg(pCbc, "Bend12"));
    LOG(INFO) << "Bend13 " << std::bitset<8>(fReadoutChipInterface->ReadChipReg(pCbc, "Bend13"));
    LOG(INFO) << "Bend14 " << std::bitset<8>(fReadoutChipInterface->ReadChipReg(pCbc, "Bend14"));
    LOG(INFO) << "HIP&TestMode " << std::bitset<8>(fReadoutChipInterface->ReadChipReg(pCbc, "HIP&TestMode")) << RESET;
}

void StubTool::maskChannel(Chip* pCbc, uint8_t iChan, bool mask)
{
    uint8_t     cRegValue;
    std::string cRegName;
    uint8_t     iChanReg = (iChan - 1) / 8;
    uint8_t     old_mask = pCbc->getReg(fChannelMaskMapCBC3[iChanReg]);
    uint8_t     new_mask;
    if(mask)
        new_mask = old_mask & (~(1 << ((iChan - 1) % 8)));
    else
        new_mask = old_mask | (1 << ((iChan - 1) % 8));
    pCbc->setReg(fChannelMaskMapCBC3[iChanReg], new_mask);
    cRegValue = pCbc->getReg(fChannelMaskMapCBC3[iChanReg]);
    cRegName  = fChannelMaskMapCBC3[iChanReg];
    fReadoutChipInterface->WriteChipReg(pCbc, cRegName, cRegValue);
    if(mask)
    {
        // LOG(DEBUG) << "CBC" << +pCbc << ", Masked Channel " << +iChan << " in register " << +iChanReg << ": old mask
        // = " << std::bitset<8>(old_mask) << ", new mask = " << std::bitset<8>(new_mask);
    }
    else
    {
        // LOG(DEBUG) << "CBC" << +pCbc << ", Unmasked Channel " << +iChan << " in register " << +iChanReg << ": old
        // mask = " << std::bitset<8>(old_mask) << ", new mask = " << std::bitset<8>(new_mask);
    }
}

uint16_t StubTool::encodeId(uint8_t pFeId, uint8_t pCbcId) { return (pFeId << 8 | pCbcId); }

#endif
