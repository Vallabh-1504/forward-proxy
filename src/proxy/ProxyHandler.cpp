#include "ProxyHandler.hpp"
#include <iostream>
#include <vector>

namespace miniCDN{

void ProxyHandler::handleRequest(SOCKET client_socket, HttpRequest &request){
    std::string host = request.getHost();
    int port = 80; // HTTP default

    // 1. Prepare the request
    request.setHeader("Connection", "close");

    // 2. Connect to remote host
    std::cout << "[Proxy] Connecting to " << host << " on port" << port << "...\n";
    SOCKET remote_socket = connectToHost(host, port);
}

SOCKET ProxyHandler::connectToHost(const std::string &host, int port){
    struct addrinfo hints, *res;
    SOCKET sockfd = INVALID_SOCKET;

    ZeroMemory(&hints, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM; // TCP

    std:: string portStr = std::to_string(port);

    // Resolve DNS
    if(getaddrinfo(host.c_str(), portStr.c_str(), &hints, &res) != 0){
        return INVALID_SOCKET;
    }

    // Create socket
    sockfd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    if(sockfd == INVALID_SOCKET){
        freeaddrinfo(res);
        return INVALID_SOCKET;
    }

    // connect
    if(connect(sockfd, res->ai_addr, (int)res->ai_addrlen) == SOCKET_ERROR){
        closesocket(sockfd);
        sockfd = INVALID_SOCKET;
    }

    freeaddrinfo(res);
    return sockfd;
}

} // namespace miniCDN