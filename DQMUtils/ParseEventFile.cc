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

    std::string rawFileName  = fRawFileFolderPath + "/" + "run_" + runString + "_Board" + boardIdString + ".raw";
    std::string rootFileName = fRawFileFolderPath + "/" + "run_" + runString + "_Board" + boardIdString + ".root";

    TFile* file = new TFile(rootFileName.c_str(), "RECREATE");
    TTree* tree = new TTree("Events", "Events");

    FileHandler theFileHandler(rawFileName, 'r');
    FileHeader  theFileHeader;
    bool        isHeaderPresent = theFileHandler.getHeader(theFileHeader);
    if(isHeaderPresent)
    {
        auto toHex = [](uint32_t value) {
            std::ostringstream oss;
            oss << std::hex << std::setw(8) << std::setfill('0') << std::uppercase << value;
            return oss.str();
        };

        TObjString theType(theFileHeader.fType.c_str());
        theType.Write("fType");

        TObjString theVersionMajor(toHex(theFileHeader.fVersionMajor).c_str());
        theVersionMajor.Write("fVersionMajor");

        TObjString theVersionMinor(toHex(theFileHeader.fVersionMinor).c_str());
        theVersionMinor.Write("fVersionMinor");

        TObjString theBeId(std::to_string(theFileHeader.fBeId).c_str());
        theBeId.Write("fBeId");

        std::string theEventTypeString = "UNKNOWN";
        switch (theFileHeader.fEventType)
        {
        case EventType::VR:
            theEventTypeString = "VR";
            break;
        case EventType::ZS:
            theEventTypeString = "ZS";
            break;
        
        default:
            break;
        }
        
        TObjString theEventType(theEventTypeString.c_str());
        theEventType.Write("fEventType");

        std::string theCICeventTypeString = "UNKNOWN";
        switch (theFileHeader.fCICeventType)
        {
        case CICeventType::Sparsified:
            theCICeventTypeString = "Sparsified";
            break;
        case CICeventType::Unsparsified:
            theCICeventTypeString = "Unsparsified";
            break;
        }

        TObjString theCICeventType(theCICeventTypeString.c_str());
        theCICeventType.Write("fCICeventType");
    }
    else
    {
        LOG(WARNING) << WARNING_FORMAT << "ParseEventFile::parseFile -> No valid header found for Run " << fRunNumber << " Board " << theBoard->getId() << RESET;
    }

    auto theData = theFileHandler.readFile();
    if(theData.size() == 0)
    {
        LOG(WARNING) << WARNING_FORMAT << "ParseEventFile::parseFile -> data vector is empty for Run " << fRunNumber << " Board " << theBoard->getId() << RESET;
        return false;
    }

    size_t currentEventStart = (isHeaderPresent ? FileHeader::fHeaderSize : 0);

    LOG(INFO) << BOLDYELLOW << "Parsing completed for Run " << fRunNumber << " Board " << theBoard->getId() << RESET;

    if(theBoard->getFirstObject()->getFrontEndType() == FrontEndType::OuterTrackerPS) { fillEventTreePS(tree, theBoard, theData, currentEventStart); }
    else if(theBoard->getFirstObject()->getFrontEndType() == FrontEndType::OuterTracker2S) { fillEventTree2S(tree, theBoard, theData, currentEventStart, theFileHeader.fCICeventType == CICeventType::Sparsified); }
    else
    {
        LOG(ERROR) << ERROR_FORMAT << "ParseEventFile::parseFile -> Unsupported FrontEndType for Run " << fRunNumber << " Board " << theBoard->getId() << RESET;
        return false;
    }

    tree->Write();
    file->Close();

    return true;
}

void ParseEventFile::fillEventTreePS(TTree* tree, const BeBoard* theBoard, const std::vector<uint32_t>& theData, size_t currentEventStart)
{
    size_t theDataSize = theData.size();

    BoardEventPS theBoardEventPS;
    tree->Branch("BoardEventPS", &theBoardEventPS);

    while(currentEventStart < theDataSize)
    {
        size_t eventSize = (theData.at(currentEventStart) & 0xFFFF) * 4;
        if(currentEventStart + eventSize >= theDataSize) break;
        std::vector<uint32_t> theEventData(theData.begin() + currentEventStart, theData.begin() + currentEventStart + eventSize);
        D19cCic2Event         theEventParsed(theBoard, theEventData, true);

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
                        theSSAL1Event.fChipEventInfo.fChipId             = theChip->getId();
                        theSSAL1Event.fChipEventInfo.fIsL1ErrorFlagSet   = theEventParsed.IsL1ErrorSet(theHybrid->getId(), theChip->getId());
                        theSSAL1Event.fChipEventInfo.fIsStubErrorFlagSet = theEventParsed.IsStubErrorSet(theHybrid->getId(), theChip->getId());

                        for(auto theCluster: theEventParsed.GetStripClusters(theHybrid->getId(), theChip->getId())) { theSSAL1Event.fClusterList.push_back(theCluster.fStripClusterPS); }
                        theHybridL1EventPS.fSSAeventList.push_back(theSSAL1Event);
                    }
                    else
                    {
                        MPAevent theMPAL1Event;
                        theMPAL1Event.fChipEventInfo.fChipId             = theChip->getId();
                        theMPAL1Event.fChipEventInfo.fIsL1ErrorFlagSet   = theEventParsed.IsL1ErrorSet(theHybrid->getId(), theChip->getId());
                        theMPAL1Event.fChipEventInfo.fIsStubErrorFlagSet = theEventParsed.IsStubErrorSet(theHybrid->getId(), theChip->getId());

                        for(auto theCluster: theEventParsed.GetPixelClusters(theHybrid->getId(), theChip->getId())) { theMPAL1Event.fClusterList.push_back(theCluster.fPixelClusterPS); }

                        for(auto theStubHandler: theEventParsed.StubVector(theHybrid->getId(), theChip->getId())) { theMPAL1Event.fStubList.push_back(theStubHandler.fStub); }
                        theHybridL1EventPS.fMPAeventList.push_back(theMPAL1Event);
                    }
                }
                theBoardEventPS.fHybrideventList.push_back(theHybridL1EventPS);
            }
        }

        tree->Fill();

        currentEventStart += eventSize;
    }
}

void ParseEventFile::fillEventTree2S(TTree* tree, const BeBoard* theBoard, const std::vector<uint32_t>& theData, size_t currentEventStart, bool isSparsified)
{
    size_t theDataSize = theData.size();

    BoardEvent2S theBoardEvent2S;
    tree->Branch("BoardEvent2S", &theBoardEvent2S);

    while(currentEventStart < theDataSize)
    {
        size_t eventSize = (theData.at(currentEventStart) & 0xFFFF) * 4;
        if(currentEventStart + eventSize >= theDataSize) break;
        std::vector<uint32_t> theEventData(theData.begin() + currentEventStart, theData.begin() + currentEventStart + eventSize);
        D19cCic2Event         theEventParsed(theBoard, theEventData, isSparsified);

        theBoardEvent2S.fHybrideventList.clear();
        theBoardEvent2S.fBoardEventInfo = theEventParsed.getBoardEventInfo();

        for(auto theOpticalGroup: *theBoard)
        {
            for(auto theHybrid: *theOpticalGroup)
            {
                HybridEvent2S theHybridL1Event2S;
                theHybridL1Event2S.fHybridL1EventInfo = theEventParsed.getHybridL1EventInfoHandler(theHybrid->getId()).fHybridL1EventInfo;

                for(auto theChip: *theHybrid)
                {
                    CBCevent theCBCL1Event;
                    theCBCL1Event.fChipEventInfo.fChipId             = theChip->getId();
                    theCBCL1Event.fChipEventInfo.fIsL1ErrorFlagSet   = theEventParsed.IsL1ErrorSet(theHybrid->getId(), theChip->getId());
                    theCBCL1Event.fChipEventInfo.fIsStubErrorFlagSet = theEventParsed.IsStubErrorSet(theHybrid->getId(), theChip->getId());

                    for(auto theCluster: theEventParsed.getClusters(theHybrid->getId(), theChip->getId())) { theCBCL1Event.fClusterList.push_back(theCluster.fCluster2S); }

                    for(auto theStubHandler: theEventParsed.StubVector(theHybrid->getId(), theChip->getId())) { theCBCL1Event.fStubList.push_back(theStubHandler.fStub); }
                    theHybridL1Event2S.fCBCeventList.push_back(theCBCL1Event);
                }
                theBoardEvent2S.fHybrideventList.push_back(theHybridL1Event2S);
            }
        }

        tree->Fill();

        currentEventStart += eventSize;
    }
}