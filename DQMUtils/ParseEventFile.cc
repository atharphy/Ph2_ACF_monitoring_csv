#include "DQMUtils/ParseEventFile.h"
#include "HWDescription/BeBoard.h"
#include "Utils/D19cCic2Event.h"
#include "Utils/FileHandler.h"
#include "Utils/FileHeader.h"
#include <TFile.h>
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

    uint32_t theEventCount;
    tree->Branch("eventId", &theEventCount, "eventId/I");
    std::vector<HybridL1EventInfo> theHybridL1EventInfoList;
    tree->Branch("HybridL1EventInfo", &theHybridL1EventInfoList);

    FileHandler theFileHandler(rawFileName, 'r');
    FileHeader  theFileHeader;
    bool        isHeaderPresent = theFileHandler.getHeader(theFileHeader);

    // std::cout << __PRETTY_FUNCTION__ << "[" << __LINE__ << "] theFileHeader.fType = " << theFileHeader.fType << std::endl;
    // std::cout << __PRETTY_FUNCTION__ << "[" << __LINE__ << "] theFileHeader.fVersionMajor = " << theFileHeader.fVersionMajor << std::endl;
    // std::cout << __PRETTY_FUNCTION__ << "[" << __LINE__ << "] theFileHeader.fVersionMinor = " << theFileHeader.fVersionMinor << std::endl;
    // std::cout << __PRETTY_FUNCTION__ << "[" << __LINE__ << "] theFileHeader.fBeId = " << theFileHeader.fBeId << std::endl;
    // std::cout << __PRETTY_FUNCTION__ << "[" << __LINE__ << "] theFileHeader.fNchip = " << theFileHeader.fNchip << std::endl;
    // std::cout << __PRETTY_FUNCTION__ << "[" << __LINE__ << "] theFileHeader.fEventSize = " << theFileHeader.fEventSize << std::endl;

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

        theHybridL1EventInfoList.clear();

        theEventCount = theEventParsed.GetEventCount();

        for(auto theOpticalGroup: *theBoard)
        {
            for(auto theHybrid: *theOpticalGroup)
            {
                theEventParsed.getHybridL1EventInfoHandler(theHybrid->getId()).print();
                std::cout << std::endl;
                theHybridL1EventInfoList.push_back(theEventParsed.getHybridL1EventInfoHandler(theHybrid->getId()).fHybridL1EventInfo);
            }
        }

        tree->Fill();
        
        currentEventStart += eventSize;
        break;
    }


    LOG(INFO) << BOLDYELLOW << "Parsing completed for Run " << fRunNumber << " Board " << theBoard->getId() << RESET;

    tree->Write();
    file->Close();

    return true;
}
