#ifndef TCPSERVER_HPP
#define TCPSERVER_HPP

// #include <WinSock2.h>
// #include <WS2tcpip.h>
// #include <string>


#include <WinSock2.h>
#include <WinDef.h>
#include <WS2tcpip.h>
#include <tchar.h>

#include <string>


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

    void setupSocket();
    void handleClient(SOCKET client_socket);
    void cleanup();

};

} // namespace miniCDN


#endif