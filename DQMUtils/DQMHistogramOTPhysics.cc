#include "DQMUtils/DQMHistogramOTPhysics.h"
#include "DQMUtils/ParseEventFile.h"
#include "HWDescription/Definition.h"
#include "Utils/ContainerFactory.h"
#include "Utils/ContainerSerialization.h"
#include <filesystem>
#include <regex>

using namespace Ph2_HwDescription;

void DQMHistogramOTPhysics::book(TFile* theOutputFile, DetectorContainer& theDetectorStructure, const Ph2_Parser::SettingsMap& settingsMap)
{
    std::string theFullRootFileName = std::string(std::getenv("PH2ACF_BASE_DIR")) + "/" + theOutputFile->GetName();
    auto        extractRunNumber    = [](const std::string& path) -> int
    {
        std::regex  re("_(\\d+)(?=/)");
        std::smatch match;

        if(std::regex_search(path, match, re)) { return std::stoi(match[1]); }
        return 0;
    };

    fRunNumber = extractRunNumber(theFullRootFileName);
    std::filesystem::path thePath(theFullRootFileName);
    fResultDirectoryName = thePath.parent_path().string();

    fDetectorContainer = &theDetectorStructure;
}

bool DQMHistogramOTPhysics::fill(std::string& inputStream) { return false; }

void DQMHistogramOTPhysics::process()
{
    ParseEventFile parser(fResultDirectoryName, fRunNumber);
    parser.parseEventFiles(fDetectorContainer);
}
