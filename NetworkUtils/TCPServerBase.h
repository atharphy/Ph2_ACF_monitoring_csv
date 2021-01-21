#ifndef _ots_TCPServerBase_h_
#define _ots_TCPServerBase_h_

#include "../NetworkUtils/TCPSocket.h"
#include <future>
#include <map>
#include <string>
#include <vector>

// namespace ots
//{

class TCPServerBase : public TCPSocket
{
  public:
    TCPServerBase(int serverPort, unsigned int maxNumberOfClients);
    virtual ~TCPServerBase(void);

    void startAccept(void);
    void broadcastPacket(const std::string& message);
    void broadcast(const std::string& message);
    void broadcast(const std::vector<char>& message);

  protected:
    virtual void acceptConnections() = 0;

    void closeClientSocket(int socket);
    template <class T>
    T* acceptClient()
    {
        int socketId = accept();
        fConnectedClients.emplace(socketId, new T(socketId));
        return dynamic_cast<T*>(fConnectedClients[socketId]);
    }

    std::promise<bool>        fAcceptPromise;
    std::map<int, TCPSocket*> fConnectedClients;
    const int                 E_SHUTDOWN = 0;

  private:
    void closeClientSockets(void);
    int  accept(void);
    void shutdownAccept(void);

    const int         fMaxConnectionBacklog = 5;
    unsigned int      fMaxNumberOfClients;
    std::atomic_bool  fAccept;
    std::future<bool> fAcceptFuture;
};

//}

#endif
