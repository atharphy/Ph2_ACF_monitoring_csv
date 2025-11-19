#ifndef ParseEventFile_h
#define ParseEventFile_h

#include <memory>
#include <string>

namespace Ph2_HwDescription
{
class BeBoard;
}
class DetectorContainer;

class ParseEventFile
{
  public:
    ParseEventFile(const std::string& rawFileFolderPath, int runNumber);
    ~ParseEventFile() = default;
    void parseEventFiles(DetectorContainer* theDetectorContainer);

  private:
    bool        parseBoardFile(const Ph2_HwDescription::BeBoard* theBoard);
    std::string fRawFileFolderPath;
    int         fRunNumber;
};

#endif