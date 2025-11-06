#include "DQMUtils/DQMHistogramOTPhysics.h"
#include "HWDescription/Definition.h"
#include "Utils/ContainerFactory.h"
#include "Utils/ContainerSerialization.h"

#include "Utils/PSSync.h"

using namespace Ph2_HwDescription;

void DQMHistogramOTPhysics::book(TFile* theOutputFile, DetectorContainer& theDetectorStructure, const Ph2_Parser::SettingsMap& settingsMap) {}

bool DQMHistogramOTPhysics::fill(std::string& inputStream) { return false; }

void DQMHistogramOTPhysics::process() {}
