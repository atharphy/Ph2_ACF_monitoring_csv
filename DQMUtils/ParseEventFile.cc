#include "DQMUtils/ParseEventFile.h"
#include "HWDescription/BeBoard.h"
#include "Utils/D19cCic2Event.h"
#include "Utils/FileHandler.h"
#include "Utils/FileHeader.h"
#include <TFile.h>
#include <TObjString.h>
#include <TTree.h>
#include <fstream>
#include <iostream>

using namespace Ph2_HwDescription;
using namespace Ph2_HwInterface;

ParseEventFile::ParseEventFile(const std::string& rawFileFolderPath, int runNumber) : fRawFileFolderPath(rawFileFolderPath), fRunNumber(runNumber) {}

void ParseEventFile::parseEventFiles(DetectorContainer* theDetectorContainer)
{
    for(auto theBoard: *theDetectorContainer) { parseBoardFile(theBoard); }
}

bool ParseEventFile::parseBoardFile(const BeBoard* theBoard)
{
    LOG(INFO) << BOLDYELLOW << "Parsing event from Run " << fRunNumber << " Board " << theBoard->getId() << RESET;
    char runString[7];
    sprintf(runString, "%06d", fRunNumber);

    char boardIdString[4];
    sprintf(boardIdString, "%03d", theBoard->getId() & 0x1FF);

    std::string rawFileName = fRawFileFolderPath + "/" + "run_" + runString + "_Board" + boardIdString + ".raw";
    std::string rootFileName = fRawFileFolderPath + "/" + "run_" + runString + "_Board" + boardIdString + ".root";

    TFile *file = new TFile(rootFileName.c_str(), "RECREATE");
    TTree *tree = new TTree("Events", "Events");

    BoardEventPS theBoardEventPS;
    tree->Branch("BoardEventPS", &theBoardEventPS);

    FileHandler theFileHandler(rawFileName, 'r');
    FileHeader  theFileHeader;
    bool        isHeaderPresent = theFileHandler.getHeader(theFileHeader);

    auto   theData     = theFileHandler.readFile();
    size_t theDataSize = theData.size();
    if(theData.size() == 0)
    {
        LOG(WARNING) << WARNING_FORMAT << "ParseEventFile::parseFile -> data vector is empty for Run " << fRunNumber << " Board " << theBoard->getId() << RESET;
        return false;
    }

    // std::vector<std::unique_ptr<D19cCic2Event>> fEventList;

    size_t currentEventStart = (isHeaderPresent ? FileHeader::fHeaderSize : 0);

    while(currentEventStart < theDataSize)
    {
        size_t eventSize = (theData.at(currentEventStart) & 0xFFFF) * 4;
        if(currentEventStart + eventSize >= theDataSize) break;
        std::vector<uint32_t> theEventData(theData.begin() + currentEventStart, theData.begin() + currentEventStart + eventSize);
        D19cCic2Event theEventParsed(theBoard, theEventData);

        theBoardEventPS.fHybrideventList.clear();
        theBoardEventPS.fBoardEventInfo = theEventParsed.getBoardEventInfo();

        for(auto theOpticalGroup: *theBoard)
        {
            for(auto theHybrid: *theOpticalGroup)
            {
                HybridEventPS theHybridL1EventPS;
                theHybridL1EventPS.fHybridL1EventInfo = theEventParsed.getHybridL1EventInfoHandler(theHybrid->getId()).fHybridL1EventInfo;

                for(auto theChip: *theHybrid)
                {
                    if(theChip->getId() < 8)
                    {
                        SSAevent theSSAL1Event;
                        theSSAL1Event.fChipEventInfo.fChipId = theChip->getId();
                        theSSAL1Event.fChipEventInfo.fIsL1ErrorFlagSet = theEventParsed.IsL1ErrorSet(theHybrid->getId(), theChip->getId());
                        theSSAL1Event.fChipEventInfo.fIsStubErrorFlagSet = theEventParsed.IsStubErrorSet(theHybrid->getId(), theChip->getId());
    
                        for(auto theCluster : theEventParsed.GetStripClusters(theHybrid->getId(), theChip->getId()))
                        {
                            theSSAL1Event.fClusterList.push_back(theCluster.fStripClusterPS);
                        }
                        theHybridL1EventPS.fSSAeventList.push_back(theSSAL1Event);
                    }
                    else
                    {
                        MPAevent theMPAL1Event;
                        theMPAL1Event.fChipEventInfo.fChipId = theChip->getId();
                        theMPAL1Event.fChipEventInfo.fIsL1ErrorFlagSet = theEventParsed.IsL1ErrorSet(theHybrid->getId(), theChip->getId());
                        theMPAL1Event.fChipEventInfo.fIsStubErrorFlagSet = theEventParsed.IsStubErrorSet(theHybrid->getId(), theChip->getId());
    
                        for(auto theCluster : theEventParsed.GetPixelClusters(theHybrid->getId(), theChip->getId()))
                        {
                            theMPAL1Event.fClusterList.push_back(theCluster.fPixelClusterPS);
                        }

                        for(auto theStubHandler : theEventParsed.StubVector(theHybrid->getId(), theChip->getId()))
                        {
                            theMPAL1Event.fStubList.push_back(theStubHandler.fStub);
                        }
                        theHybridL1EventPS.fMPAeventList.push_back(theMPAL1Event);
                    }
                }
                theBoardEventPS.fHybrideventList.push_back(theHybridL1EventPS);
            }
        }

        tree->Fill();
        
        currentEventStart += eventSize;
    }

    LOG(INFO) << BOLDYELLOW << "Parsing completed for Run " << fRunNumber << " Board " << theBoard->getId() << RESET;

    tree->Write();
    file->Close();

    return true;
}
