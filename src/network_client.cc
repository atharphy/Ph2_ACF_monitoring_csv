#include "../NetworkUtils/TCPClient.h"
#include "../NetworkUtils/TCPSubscribeClient.h"
#include "../Utils/easylogging++.h"

#include <chrono>
#include <future>
#include <iostream>
#include <thread>

INITIALIZE_EASYLOGGINGPP

class MiddlewareInterface : public TCPClient
{
  public:
    MiddlewareInterface(std::string serverIP, int serverPort) : TCPClient(serverIP, serverPort), fRunning(true), fAccept(true), fAcceptFuture(fAcceptPromise.get_future()) {}
    virtual ~MiddlewareInterface(void) { ; }

    void initialize(void)
    {
        if(!TCPClient::connect())
        {
            std::cout << __PRETTY_FUNCTION__ << "ERROR CAN'T CONNECT TO SERVER!" << std::endl;
            abort();
        }
        /*
        std::cout << __PRETTY_FUNCTION__ << "Client connected!" << std::endl;
        std::string megaStringA(9994, 'a');
        std::string megaStringB(9994, 'b');
        std::string readBufferA;
        std::string readBufferB;
        std::cout << __PRETTY_FUNCTION__ << "Sending and receiving A!" << std::endl;
        readBufferA = TCPClient::sendAndReceivePacket(megaStringA);
        std::cout << __PRETTY_FUNCTION__ << "Sending and receiving B!" << std::endl;
        readBufferB = TCPClient::sendAndReceivePacket(megaStringB);
        for (int l = 0; l < 10; l++)
        {
            for (int i = 10; i < 20; i++)
            {
                std::cout << __PRETTY_FUNCTION__ << "Sending A! " << i - 10 << std::endl;
                TCPClient::sendPacket(megaStringA + std::to_string(i));
                //std::cout << __PRETTY_FUNCTION__ << "Sending B!" << std::endl;
                //TCPClient::sendPacket(megaStringB);
            }
            for (int i = 10; i < 20; i++)
            {
                //std::cout << __PRETTY_FUNCTION__ << "Receiving A! " << i << std::endl;
                readBufferA = TCPClient::receivePacket();
                //std::cout << __PRETTY_FUNCTION__ << "Receiving B! " << i << std::endl;
                //readBufferB = TCPClient::receivePacket();
            }
        }
*/
        std::cout << __PRETTY_FUNCTION__
                  << "DONE WITH Initialize-"
                  //<< readBufferA
                  << "-" << std::endl;
    }

    //========================================================================================================================
    void configure(std::string calibrationName, std::string configurationFilePath)
    {
        std::string readBuffer;
        try
        {
            std::string readBuffer = TCPClient::sendAndReceivePacket("Configure,Calibration:" + calibrationName + ",ConfigurationFile:" + configurationFilePath);
        }
        catch(const std::exception& e)
        {
            std::cerr << e.what() << '\n';
            std::cout << __PRETTY_FUNCTION__ << "Can't send packets to server...Error: " << e.what() << std::endl;
        }
        std::cout << __PRETTY_FUNCTION__ << "DONE WITH Configure-" << readBuffer << "-" << std::endl;
    }

    //========================================================================================================================
    void start(void)
    {
        fRunning  = true;
        fListener = new TCPSubscribeClient("127.0.0.1", 6000);
        fAccept   = true;
        std::thread thread(&MiddlewareInterface::running, this);
        thread.detach();
        std::cout << __PRETTY_FUNCTION__ << "Sending Start!" << std::endl;
        std::string readBuffer;
        try
        {
            readBuffer = TCPClient::sendAndReceivePacket("Start");
        }
        catch(const std::exception& e)
        {
            std::cerr << e.what() << '\n';
            std::cout << __PRETTY_FUNCTION__ << "Can't send packets to server...Error: " << e.what() << std::endl;
        }
        std::cout << __PRETTY_FUNCTION__ << "DONE WITH Start-" << readBuffer << "-" << std::endl;
    }

    //========================================================================================================================
    void stop(void)
    {
        std::cout << __PRETTY_FUNCTION__ << "Sending Stop!" << std::endl;
        std::string readBuffer;
        try
        {
            readBuffer = TCPClient::sendAndReceivePacket("Stop");
        }
        catch(const std::exception& e)
        {
            std::cerr << e.what() << '\n';
            std::cout << __PRETTY_FUNCTION__ << "Can't send packets to server...Error: " << e.what() << std::endl;
        }

        std::cout << __PRETTY_FUNCTION__ << "DONE WITH Stop-" << readBuffer << "-" << std::endl;
        std::this_thread::sleep_for(std::chrono::seconds(5));
        fAccept = false;
        while(fAcceptFuture.wait_for(std::chrono::milliseconds(1000)) != std::future_status::ready) std::cout << __PRETTY_FUNCTION__ << "Still running" << std::endl;
        if(fListener != nullptr) delete fListener;
        fRunning = false;
    }

    //========================================================================================================================
    void running()
    {
        if(!fListener->connect())
        {
            std::cout << __PRETTY_FUNCTION__ << "ERROR CAN'T CONNECT TO SERVER!" << std::endl;
            abort();
        }
        std::cout << __PRETTY_FUNCTION__ << "DQM connected!" << std::endl;
        while(fAccept)
        {
            std::string tmpDataBuffer;
            try
            {
                tmpDataBuffer = fListener->receive<std::string>();
            }
            catch(const std::exception& e)
            {
                std::cout << __PRETTY_FUNCTION__ << "Error: " << e.what() << std::endl;
                break;
            }

            std::cout << __PRETTY_FUNCTION__ << "Receiving-" << tmpDataBuffer << "- length: " << tmpDataBuffer.size() << std::endl;
            std::this_thread::sleep_for(std::chrono::seconds(1));
            // delete fListener;
            // fListener = nullptr;
            // fAccept = false;
        }
        fAcceptPromise.set_value(true);
    };
    bool isRunning(void) { return fRunning; }

  private:
    bool                fRunning;
    TCPSubscribeClient* fListener;
    std::promise<bool>  fAcceptPromise;
    std::atomic_bool    fAccept;
    std::future<bool>   fAcceptFuture;
};

int main(int argc, char* argv[])
{
    MiddlewareInterface theMiddlewareInterface("127.0.0.1", 5000);
    // MiddlewareInterface theMiddlewareInterface("131.225.86.69",5000);
    // MiddlewareInterface theMiddlewareInterface("192.168.0.100",5000);
    // MiddlewareInterface theMiddlewareInterface("kenny01.dhcp.fnal.gov",5000);
    // MiddlewareInterface theMiddlewareInterface("ciao.dhcp.fnal.gov",5000);
    theMiddlewareInterface.initialize();
    // std::this_thread::sleep_for(std::chrono::seconds(1));
    theMiddlewareInterface.start();
    std::this_thread::sleep_for(std::chrono::seconds(10));
    theMiddlewareInterface.stop();
    while(theMiddlewareInterface.isRunning())
    {
        std::cout << __PRETTY_FUNCTION__ << "Middleware interface running" << std::endl;
        std::this_thread::sleep_for(std::chrono::seconds(5));
    };

    return EXIT_SUCCESS;
}
