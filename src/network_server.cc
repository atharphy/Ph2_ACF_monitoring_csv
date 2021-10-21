#include "../NetworkUtils/TCPPublishServer.h"
#include "../NetworkUtils/TCPServer.h"
#include "../Utils/easylogging++.h"

#include <future>
#include <iostream>

INITIALIZE_EASYLOGGINGPP

#define PORT 5000

class MiddlewareController : public TCPServer
{
  public:
    MiddlewareController(int serverPort) : TCPServer(serverPort, 1), fRunning(true), fAccept(true), fAcceptFuture(fAcceptPromise.get_future())
    {
        std::cout << __PRETTY_FUNCTION__ << "Totally done with the constructor" << std::endl;
        std::cout << __PRETTY_FUNCTION__ << "Totally done with the constructor" << std::endl;
        std::cout << __PRETTY_FUNCTION__ << "Totally done with the constructor" << std::endl;
        std::cout << __PRETTY_FUNCTION__ << "Totally done with the constructor" << std::endl;
    }
    virtual ~MiddlewareController(void) { ; }

    void send(const std::string& message) { broadcastPacket(message); }
    //========================================================================================================================
    // virtual function to interpret messages
    virtual std::string interpretMessage(const std::string& buffer) override
    {
        // std::cout << __PRETTY_FUNCTION__ << "RECEIVED: " << buffer << std::endl;

        if(buffer == "Initialize") // changing the status changes the mode in threadMain (BBC) function.
        { return "InitializeDone"; }
        if(buffer == "Configure") // changing the status changes the mode in threadMain (BBC) function.
        { return "ConfiguereDone"; }
        if(buffer.substr(0, 5) == "Start") // changing the status changes the mode in threadMain (BBC) function.
        {
            fRunning         = true;
            fNetworkStreamer = new TCPPublishServer(6000, 1);
            fAccept          = true;
            std::thread thread(&MiddlewareController::running, this);
            thread.detach();
            // currentRun_ = getVariableValue("RunNumber", buffer);
            return "StartDone";
        }
        else if(buffer.substr(0, 4) == "Stop")
        {
            // We need to think :)
            std::cout << __PRETTY_FUNCTION__ << "Closing the network publisher socket: " << getSocketId() << std::endl;
            fAccept = false;
            while(fAcceptFuture.wait_for(std::chrono::milliseconds(100)) != std::future_status::ready) std::cout << __PRETTY_FUNCTION__ << "Still running" << std::endl;
            if(fNetworkStreamer != nullptr) delete fNetworkStreamer;
            std::cout << "Run " << currentRun_ << " fRunning!" << std::endl;
            fRunning = false;
            return "StopDone";
        }
        else if(buffer == "Pause")
        {
            // We need to think :)
            std::cout << "Paused" << std::endl;
            return "PauseDone";
        }
        else if(buffer == "Resume")
        {
            // We need to think :)
            std::cout << "Resume" << std::endl;
            return "ResumeDone";
        }
        else
        {
            return buffer;
            // return "suca";
        }
    }
    bool isRunning(void) { return fRunning; }

  private:
    bool fRunning;
    void running()
    {
        int counter = 0;
        while(fAccept)
        {
            std::this_thread::sleep_for(std::chrono::seconds(1));
            std::cout << __PRETTY_FUNCTION__ << "Trying to stream: " << counter << " fAccept?" << fAccept << std::endl;
            fNetworkStreamer->broadcast(std::to_string(counter));
            std::cout << __PRETTY_FUNCTION__ << "Streamed: " << counter << " fAccept?" << fAccept << std::endl;
            ++counter;
            // fNetworkStreamer->broadcast("");
        }
        fAcceptPromise.set_value(true);
    };
    std::string currentRun_;
    std::string getVariableValue(std::string variable, std::string buffer)
    {
        size_t begin = buffer.find(variable) + variable.size() + 1;
        size_t end   = buffer.find(',', begin);
        if(end == std::string::npos) end = buffer.size();
        return buffer.substr(begin, end - begin);
    }
    TCPPublishServer*  fNetworkStreamer;
    std::promise<bool> fAcceptPromise;
    std::atomic_bool   fAccept;
    std::future<bool>  fAcceptFuture;
};

int main(int argc, char* argv[])
{
    MiddlewareController theMiddlewareController(PORT);

    while(theMiddlewareController.isRunning())
    {
        std::cout << __PRETTY_FUNCTION__ << "Network server running?" << theMiddlewareController.isRunning() << std::endl;
        std::this_thread::sleep_for(std::chrono::seconds(10));
        std::cout << __PRETTY_FUNCTION__ << "Network server running?" << theMiddlewareController.isRunning() << std::endl;
    }
    std::cout << __PRETTY_FUNCTION__ << "Network server fRunning. Exiting..." << std::endl;

    return EXIT_SUCCESS;
}
