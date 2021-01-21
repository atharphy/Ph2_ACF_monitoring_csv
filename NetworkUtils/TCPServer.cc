#include "../NetworkUtils/TCPServer.h"
#include "../NetworkUtils/TCPTransceiverSocket.h"
#include <errno.h>  // errno
#include <string.h> // errno

#include <iostream>

//========================================================================================================================
TCPServer::TCPServer(int serverPort, unsigned int maxNumberOfClients) : TCPServerBase(serverPort, maxNumberOfClients)
{
    fReceiveTimeout.tv_sec  = 0;
    fReceiveTimeout.tv_usec = 0;
    fSendTimeout.tv_sec     = 0;
    fSendTimeout.tv_usec    = 0;
    // fAcceptFuture = std::async(std::launch::async, &TCPServer::acceptConnections, this);
}

//========================================================================================================================
TCPServer::~TCPServer(void) {}

// void TCPServer::StartAcceptConnections()
// {

// }
//========================================================================================================================
// time out or protection for this receive method?
// void TCPServer::connectClient(int fdClientSocket)
void TCPServer::connectClient(TCPTransceiverSocket* socket)
{
    while(1)
    {
        std::cout << __PRETTY_FUNCTION__ << "Waiting for message for socket  #: " << socket->getSocketId() << std::endl;
        std::string message;
        try
        {
            message = socket->receivePacket();
        }
        catch(const std::exception& e)
        {
            std::cout << __PRETTY_FUNCTION__ << "Error: " << e.what() << std::endl; // Client connection must have
                                                                                    // closed
            std::cerr << __PRETTY_FUNCTION__ << e.what() << '\n';
            TCPServerBase::closeClientSocket(socket->getSocketId());
            interpretMessage("Error: " + std::string(e.what()));
            return; // the pointer to socket has been deleted in closeClientSocket
        }

        std::cout << __PRETTY_FUNCTION__ << "Received message:-" << message << "-"
                  << "Message Length=" << message.length() << " From socket #: " << socket->getSocketId() << std::endl;
        std::string messageToClient = interpretMessage(message);

        // Send back something only if there is actually a message to be sent!
        if(messageToClient != "")
        {
            // std::cout << __PRETTY_FUNCTION__ << "Sending back message:-" << messageToClient << "-(nbytes=" <<
            // messageToClient.length() << ") to socket #: " << socket->getSocketId() << std::endl;
            socket->sendPacket(messageToClient);
        }
        // else
        // 	std::cout << __PRETTY_FUNCTION__ << "Not sending anything back to socket  #: " << socket->getSocketId() <<
        // std::endl;

        // std::cout << __PRETTY_FUNCTION__ << "After message sent now checking for more... socket #: " <<
        // socket->getSocketId() << std::endl;
    }

    std::cout << __PRETTY_FUNCTION__ << "Thread done for socket  #: " << socket->getSocketId() << std::endl;
}

//========================================================================================================================
void TCPServer::acceptConnections()
{
    // std::pair<std::unordered_map<int, TCPTransceiverSocket>::iterator, bool> element;
    while(true)
    {
        try
        {
            TCPTransceiverSocket* clientSocket = acceptClient<TCPTransceiverSocket>();
            clientSocket->setReceiveTimeout(fReceiveTimeout.tv_sec, fReceiveTimeout.tv_usec);
            clientSocket->setSendTimeout(fSendTimeout.tv_sec, fSendTimeout.tv_usec);
            std::thread thread(&TCPServer::connectClient, this, clientSocket);
            thread.detach();
        }
        catch(int e)
        {
            std::cout << __PRETTY_FUNCTION__ << "Shutting down socket!" << std::endl;
            if(e == E_SHUTDOWN) break;
        }
    }
    fAcceptPromise.set_value(true);
}

//========================================================================================================================
void TCPServer::setReceiveTimeout(unsigned int timeoutSeconds, unsigned int timeoutMicroseconds)
{
    fReceiveTimeout.tv_sec  = timeoutSeconds;
    fReceiveTimeout.tv_usec = timeoutMicroseconds;
}

//========================================================================================================================
void TCPServer::setSendTimeout(unsigned int timeoutSeconds, unsigned int timeoutMicroseconds)
{
    fSendTimeout.tv_sec  = timeoutSeconds;
    fSendTimeout.tv_usec = timeoutMicroseconds;
}
