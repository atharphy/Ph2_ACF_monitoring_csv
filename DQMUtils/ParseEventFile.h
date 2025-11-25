#ifndef ParseEventFile_h
#define ParseEventFile_h

#include <memory>
#include <string>
#include <vector>

namespace Ph2_HwDescription
{
class BeBoard;
}
class DetectorContainer;
class TTree;

class ParseEventFile
{
  public:
    ParseEventFile(const std::string& rawFileFolderPath, int runNumber);
    ~ParseEventFile() = default;
    void parseEventFiles(DetectorContainer* theDetectorContainer);

  private:
    bool        parseBoardFile(const Ph2_HwDescription::BeBoard* theBoard);
    void        fillEventTreePS(TTree* tree, const Ph2_HwDescription::BeBoard* theBoard, const std::vector<uint32_t>& theData, size_t currentEventStart);
    void        fillEventTree2S(TTree* tree, const Ph2_HwDescription::BeBoard* theBoard, const std::vector<uint32_t>& theData, size_t currentEventStart, bool isSparsified);
    std::string fRawFileFolderPath;
    int         fRunNumber;
};

#endif