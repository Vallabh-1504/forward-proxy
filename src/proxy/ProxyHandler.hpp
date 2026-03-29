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
    // Resolve host DNS and establishes a TCP connection
    SOCKET connectToHost(const std::string &host, int port);

    // Send the ocnfigured HTTP request string to the remote server socket
    bool forwardRequest(SOCKET remote_socket, const HttpRequest &request);

    // Reads origin's response and streams it to the client socket
    void relayResponse(SOCKET remote_socket, SOCKET client_socket);
};


} // namespace miniCDN

#endif