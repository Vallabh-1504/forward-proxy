#ifndef PROXYHANDLER_HPP
#define PROXYHANDLER_HPP

#include <WinSock2.h>
#include <WS2tcpip.h>
#include <string>
#include "../http/HttpRequest.hpp"

namespace miniCDN{

class ProxyHandler{

public:
    void handleRequest(SOCKET client_socket, HttpRequest &request);

private:
    SOCKET connectToHost(const std::string &host, int port);
};


} // namespace miniCDN

#endif