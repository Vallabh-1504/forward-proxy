#ifndef PROXYHANDLER_HPP
#define PROXYHANDLER_HPP

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>

#include <string>

#include "HttpRequest.hpp"
#include "LRUCache.hpp"

namespace miniCDN{

class ProxyHandler{

public:
    ProxyHandler(LRUCache &cache);

    void handleRequest(int client_socket, HttpRequest &request);

    // HTTPS CONNECT tunnel- open a raw TCP pipe between client and origin
    // data pass untouched
    void handleConnect(int client_socket, const std::string &host, int port);

private:
    LRUCache& m_cache;

    // Resolve host DNS and establishes a TCP connection
    int connectToHost(const std::string &host, int port);

    // Send the ocnfigured HTTP request string to the remote server socket
    bool forwardRequest(int remote_socket, const HttpRequest &request);

    // Get the response from the remotre server
    std::string fetchResponse(int remote_socket);

    // send the response to client 
    void sendToClient(int client_socket, const std::string &response);

    // Bidirectional byte relay for CONNECT tunnels, using select()
    void runTunnel(int client_socket, int remote_socket);
};


} // namespace miniCDN

#endif