#ifndef __FILE_DUMPER__
#define __FILE_DUMPER__

#include <string>
#include <unordered_map>
#include "pugixml.hpp"
#include <boost/any.hpp>

class DetectorContainer;
namespace Ph2_HwDescription
{
    class BeBoard;
    class OpticalGroup;
    class Hybrid;
    class ReadoutChip;
}

class FileDumper
{
  public:
    FileDumper(const std::string& outputDirectory);
    ~FileDumper();

    void dumpConfigurationFiles(DetectorContainer* theDetectorContainer, const std::unordered_map<std::string, boost::any>& theSettingMap);

  private:
    void dumpBoardConfigurationFile(pugi::xml_node theMotherNode, Ph2_HwDescription::BeBoard* theBoardContainer);
    void dumpOpticalGroupConfigurationFile(pugi::xml_node theMotherNode, Ph2_HwDescription::OpticalGroup* theOpticalGroupContainer);
    void dumpHybridConfigurationFile(pugi::xml_node theMotherNode, Ph2_HwDescription::Hybrid* theHybridContainer);
    void dumpChipConfigurationFile(pugi::xml_node theMotherNode, Ph2_HwDescription::ReadoutChip* theReadoutChip);

    std::string fOutputDirectory;
};

#endif