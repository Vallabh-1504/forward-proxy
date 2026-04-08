#ifndef PROXYHANDLER_HPP
#define PROXYHANDLER_HPP

#include <WinSock2.h>
#include <WS2tcpip.h>
#include <string>

#include "../http/HttpRequest.hpp"
#include "../cache/LRUCache.hpp"

namespace miniCDN{

class ProxyHandler{

public:
    ProxyHandler(LRUCache &cache);

    void handleRequest(SOCKET client_socket, HttpRequest &request);

private:
    LRUCache& m_cache;

    // Resolve host DNS and establishes a TCP connection
    SOCKET connectToHost(const std::string &host, int port);

    // Send the ocnfigured HTTP request string to the remote server socket
    bool forwardRequest(SOCKET remote_socket, const HttpRequest &request);

    // Get the response from the remotre server
    std::string fetchResponse(SOCKET remote_socket);

    // send the response to client 
    void sendToClient(SOCKET client_socket, const std::string &response);
};


} // namespace miniCDN

#endif