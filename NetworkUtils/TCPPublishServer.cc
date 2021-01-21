#include "../NetworkUtils/TCPPublishServer.h"
#include "../NetworkUtils/TCPTransmitterSocket.h"

#include <iostream>

//========================================================================================================================
TCPPublishServer::TCPPublishServer(int serverPort, unsigned int maxNumberOfClients) : TCPServerBase(serverPort, maxNumberOfClients) {}

//========================================================================================================================
TCPPublishServer::~TCPPublishServer(void) {}

//========================================================================================================================
void TCPPublishServer::acceptConnections()
{
    while(true)
    {
        try
        {
            // if(fConnectedClients.size() < fMaxNumberOfClients)
            __attribute__((unused)) TCPTransmitterSocket* clientSocket = acceptClient<TCPTransmitterSocket>();
        }
        catch(int e)
        {
            std::cout << __PRETTY_FUNCTION__ << "SHUTTING DOWN SOCKET" << std::endl;
            std::cout << __PRETTY_FUNCTION__ << "SHUTTING DOWN SOCKET" << std::endl;
            std::cout << __PRETTY_FUNCTION__ << "SHUTTING DOWN SOCKET" << std::endl;

            if(e == E_SHUTDOWN) break;
        }
    }
    fAcceptPromise.set_value(true);
}
