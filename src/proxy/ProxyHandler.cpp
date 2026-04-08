#include "ProxyHandler.hpp"
#include <WinSock2.h>
#include <iostream>
#include <wingdi.h>

namespace miniCDN{

ProxyHandler::ProxyHandler(LRUCache &cache) : m_cache(cache){}

void ProxyHandler::handleRequest(SOCKET client_socket, HttpRequest &request){
    std::string host = request.getHost();
    int port = request.getPort();

    if(host.empty()){
        std::cerr << "[Proxy] Could not determine host from request.\n";
        return;
    }

    // Cache check- key is full URL sent by the browser
    std::string cacheKey = request.getUrl();
    auto cached = m_cache.get(cacheKey);
    if(cached.has_value()){
        std::cout << "[cache] HIT: " << cacheKey << "\n";
        sendToClient(client_socket, cached.value());
        return;
    }
    std::cout << "[cache] Miss: " << cacheKey << "\n";

    // 1. Prepare request headers for forwarding, add headers
    request.setHeader("Host", host); // ensure Host header is present
    request.setHeader("Connection", "close"); // ask origin to close after response, currently

    // 2. prepare request header, remove headers
    // browsers send "Proxy-Connecton: Keep-Alive", meant for proxy only and not to be forwarded to origin
    request.removeHeader("Proxy-Connection");

    // 2. Connect to remote host
    std::cout << "[Proxy] Connecting to " << host << " on port " << port << "...\n";
    SOCKET remote_socket = connectToHost(host, port);
    if(remote_socket == INVALID_SOCKET){
        std::cerr << "[Proxy] Failed to connect to " << host << "\n";
        return;
    }
    std::cout << "[Proxy] Connected to " << host << "\n";

    // set a receive timeout on remote_socket
    // as not keeping can freeze thread on recv if received broken headers
    DWORD recvTimeout = 10000;
    setsockopt(remote_socket, SOL_SOCKET, SO_RCVTIMEO, (const char*)&recvTimeout, sizeof(recvTimeout));

    // 3. Forward the HTTP request to the origin server
    bool forwardedRequest = forwardRequest(remote_socket, request);
    if(!forwardedRequest){
        std::cerr << "[Proxy] Failed to forward request to " << host << "\n";
        closesocket(remote_socket);
        return;
    }
    std::cout << "[Proxy] Request forwarded to " << host << "\n";

    // 4. Fetch the full response into memory
    std::string response = fetchResponse(remote_socket);
    closesocket(remote_socket);

    if(response.empty()){
        std::cerr << "[proxy] Empty response from " << host << "\n";
        return;
    }

    // 5. store the response from Cache
    m_cache.put(cacheKey, response);
    std::cout << "[Cache] stored response for: " << cacheKey << "\n";

    // 6. Send the response back to the client
    std::cout << "[Proxy] sending response to client from " << host << "...\n";
    sendToClient(client_socket, response);
    
    std::cout << "[Proxy] Done.\n";
}

SOCKET ProxyHandler::connectToHost(const std::string &host, int port){
    struct addrinfo hints, *res = nullptr;
    SOCKET sockfd = INVALID_SOCKET;

    ZeroMemory(&hints, sizeof(hints));
    hints.ai_family   = AF_INET;      // IPv4 default
    hints.ai_socktype = SOCK_STREAM;  // TCP

    std::string portStr = std::to_string(port);

    // Resolve DNS
    if(getaddrinfo(host.c_str(), portStr.c_str(), &hints, &res) != 0){
        std::cerr << "[Proxy] getaddrinfo failed for host: " << host << "\n";
        return INVALID_SOCKET;
    }

    // Create socket and connect
    sockfd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    if(sockfd == INVALID_SOCKET){
        freeaddrinfo(res);
        return INVALID_SOCKET;
    }

    if(connect(sockfd, res->ai_addr, (int)res->ai_addrlen) == SOCKET_ERROR){
        closesocket(sockfd);
        sockfd = INVALID_SOCKET;
    }

    freeaddrinfo(res);
    return sockfd;
}

bool ProxyHandler::forwardRequest(SOCKET remote_socket, const HttpRequest &request){
    std::string requestStr = request.toString();
    const char *data = requestStr.c_str();
    int totalLen = (int)requestStr.size();
    int totalSent = 0;

    // TCP may send in chunks -> loop to guarantee all bytes are sent
    while(totalSent < totalLen){
        int sent = send(remote_socket, data + totalSent, totalLen - totalSent, 0);
        if(sent == SOCKET_ERROR){
            std::cerr << "[Proxy] send() to origin failed: " << WSAGetLastError() << "\n";
            return false;
        }
        totalSent += sent;
    }

    return true;
}

std::string ProxyHandler::fetchResponse(SOCKET remote_socket){
    std::string response;
    char buffer[4096];

    // Read all chunks from origin into one string
    int bytes;
    while((bytes = recv(remote_socket, buffer, sizeof(buffer), 0)) > 0){
        response.append(buffer, bytes);
    }

    if(bytes < 0){
        int error = WSAGetLastError();
        if(error == WSAETIMEDOUT){
            std::cerr << "[Proxy] origin timed out mid-response\n";
        }
        else{
            std::cerr << "[Proxy] recv() from origin failed: " << WSAGetLastError() << "\n";
        }
    }
    std::cout << "[Proxy] Fetched " << response.size() << " bytes from origin.\n";
    return response;
}

void ProxyHandler::sendToClient(SOCKET client_socket, const std::string &response){
    const char *data = response.c_str();
    int totalLength = (int)response.size();
    int totalSent = 0;

    // Introduce partial send loop, TCP may not send all at once
    while(totalSent < totalLength){
        int sent = send(client_socket, data + totalSent, totalLength - totalSent, 0);
        if(sent == SOCKET_ERROR){
            std::cerr << "[Proxy] send() to client failed: " << WSAGetLastError() << "\n";
            return;
        }
        totalSent += sent;
    }

    std::cout << "[Proxy] sent " << totalLength << "bytes to client\n";
}

} // namespace miniCDN