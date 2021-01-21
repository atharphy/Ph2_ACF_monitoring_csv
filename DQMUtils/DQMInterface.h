#ifndef _DQMInterface_h_
#define _DQMInterface_h_

#include <future>
#include <vector>

class TCPSubscribeClient;
class DQMHistogramBase;
class TFile;

class DQMInterface
{
  public:
    DQMInterface();
    ~DQMInterface(void);

    void configure(std::string const& calibrationName, std::string const& configurationFilePath);
    void startProcessingData(std::string const& runNumber);
    void stopProcessingData(void);
    void pauseProcessingData(void) {}
    void resumeProcessingData(void) {}

    bool running(void);

  private:
    void                           destroy(void);
    void                           destroyHistogram(void);
    TCPSubscribeClient*            fListener;
    std::vector<DQMHistogramBase*> fDQMHistogrammerVector;
    std::vector<char>              fDataBuffer;
    bool                           fRunning;
    std::future<bool>              fRunningFuture;
    TFile*                         fOutputFile;
};

#endif
