#ifndef TCPSERVER_HPP
#define TCPSERVER_HPP

#include <WinSock2.h>
#include <WinDef.h>
#include <WS2tcpip.h>
#include <tchar.h>

#include "../cache/LRUCache.hpp"
#include "../threadpool/Threadpool.hpp"

namespace miniCDN{

class TcpServer{
public:
    TcpServer(int port);
    ~TcpServer();

    void start();

private:
    int m_port;
    SOCKET m_server_socket;
    sockaddr_in m_server_addr;
    WSADATA m_wsaData;

    LRUCache m_cache;
    Threadpool m_threapool;

    void setupSocket();
    void handleClient(SOCKET client_socket);
    void cleanup();
};

} // namespace miniCDN

#endif