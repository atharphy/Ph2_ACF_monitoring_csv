#ifndef _MonitorDQMInterface_h_
#define _MonitorDQMInterface_h_

#include <future>
#include <vector>

class TCPSubscribeClient;
class MonitorDQMPlotBase;
class TFile;

class MonitorDQMInterface
{
  public:
    MonitorDQMInterface();
    ~MonitorDQMInterface(void);

    void configure(std::string const& monitorName, std::string const& configurationFilePath);
    void startProcessingData();
    void stopProcessingData(void);
    void pauseProcessingData(void) {};
    void resumeProcessingData(void) {};

    bool running(void);

  private:
    void                             destroy(void);
    void                             destroyDQMs(void);
    TCPSubscribeClient*              fListener;
    std::vector<MonitorDQMPlotBase*> fMonitorDQMVector;
    std::vector<char>                fDataBuffer;
    bool                             fRunning;
    std::future<bool>                fRunningFuture;
    TFile*                           fOutputFile;
};

#endif
