#include "ProxyHandler.hpp"

#include <iostream>
#include <cerrno>
#include <cstring>
#include <algorithm>

namespace miniCDN{

ProxyHandler::ProxyHandler(LRUCache &cache) : m_cache(cache){}

void ProxyHandler::handleRequest(int client_socket, HttpRequest &request){
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
    int remote_socket = connectToHost(host, port);
    if(remote_socket == -1){
        std::cerr << "[Proxy] Failed to connect to " << host << "\n";
        return;
    }
    std::cout << "[Proxy] Connected to " << host << "\n";

    // set a receive timeout on remote_socket
    // as not keeping can freeze thread on recv if received broken headers
    struct timeval recvTimeout{10, 0};
    setsockopt(remote_socket, SOL_SOCKET, SO_RCVTIMEO, &recvTimeout, sizeof(recvTimeout));

    // 3. Forward the HTTP request to the origin server
    bool forwardedRequest = forwardRequest(remote_socket, request);
    if(!forwardedRequest){
        std::cerr << "[Proxy] Failed to forward request to " << host << "\n";
        close(remote_socket);
        return;
    }
    std::cout << "[Proxy] Request forwarded to " << host << "\n";

    // 4. Fetch the full response into memory
    std::string response = fetchResponse(remote_socket);
    close(remote_socket);

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

int ProxyHandler::connectToHost(const std::string &host, int port){
    struct addrinfo hints, *res = nullptr;

    memset(&hints, 0, sizeof(hints));
    hints.ai_family   = AF_INET;      // IPv4 default
    hints.ai_socktype = SOCK_STREAM;  // TCP

    std::string portStr = std::to_string(port);

    // Resolve DNS
    if(getaddrinfo(host.c_str(), portStr.c_str(), &hints, &res) != 0){
        std::cerr << "[Proxy] getaddrinfo failed for host: " << host << "\n";
        return -1;
    }

    int sockfd = -1;
    // Create socket and connect
    sockfd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    if(sockfd == -1){
        freeaddrinfo(res);
        return -1;
    }

    if(connect(sockfd, res->ai_addr, (int)res->ai_addrlen) == -1){
        close(sockfd);
        sockfd = -1;
    }

    freeaddrinfo(res);
    return sockfd;
}

bool ProxyHandler::forwardRequest(int remote_socket, const HttpRequest &request){
    std::string requestStr = request.toString();
    const char *data = requestStr.c_str();
    int totalLen = (int)requestStr.size();
    int totalSent = 0;

    // TCP may send in chunks -> loop to guarantee all bytes are sent
    while(totalSent < totalLen){
        int sent = send(remote_socket, data + totalSent, totalLen - totalSent, 0);
        if(sent == -1){
            std::cerr << "[Proxy] send() to origin failed: " << errno << "\n";
            return false;
        }
        totalSent += sent;
    }

    return true;
}

std::string ProxyHandler::fetchResponse(int remote_socket){
    std::string response;
    char buffer[4096];

    // Read all chunks from origin into one string
    int bytes;
    while((bytes = recv(remote_socket, buffer, sizeof(buffer), 0)) > 0){
        response.append(buffer, bytes);
    }

    if(bytes < 0){
        int error = errno;
        if(error == EAGAIN || error == EWOULDBLOCK){
            std::cerr << "[Proxy] origin timed out mid-response\n";
        }
        else{
            std::cerr << "[Proxy] recv() from origin failed: " << errno << "\n";
        }
    }
    std::cout << "[Proxy] Fetched " << response.size() << " bytes from origin.\n";
    return response;
}

void ProxyHandler::sendToClient(int client_socket, const std::string &response){
    const char *data = response.c_str();
    int totalLength = (int)response.size();
    int totalSent = 0;

    // Introduce partial send loop, TCP may not send all at once
    while(totalSent < totalLength){
        int sent = send(client_socket, data + totalSent, totalLength - totalSent, 0);
        if(sent == -1){
            std::cerr << "[Proxy] send() to client failed: " << errno << "\n";
            return;
        }
        totalSent += sent;
    }

    std::cout << "[Proxy] sent " << totalLength << "bytes to client\n";
}

void ProxyHandler::handleConnect(int client_socket, const std::string &host, int port){
    std::cout << "[Proxy] CONNECT request for " << host << ":" << port << "\n";

    // Opening a plain TCP connection to the origin on the requested port (usually 443).
    // TLS is handled entirely by the browser and origin
    int remote_socket = connectToHost(host, port);
    if(remote_socket == -1){
        std::cerr << "[Proxy] CONNECT failed: could not reach " << host << ":" << port << "\n";
        return;
    }

    // Tell browser the tunnel is ready.
    // After this, browser starts TLS handshake directly and all subsequent bytes are encrypted and we forward them blindly (no caching possible)
    std::string ok = "HTTP/1.1 200 Connection Established\r\n\r\n";

    send(client_socket, ok.c_str(), (int)ok.size(), 0);
    std::cout << "[Proxy] CONNECT tunnel open: " << host << ":" << port << "\n";

    runTunnel(client_socket, remote_socket);
    std::cout << "[Proxy] CONNECT tunnel closed: " << host << ":" << port << "\n";

    close(remote_socket);
}

void ProxyHandler::runTunnel(int client_socket, int remote_socket){
    char buffer[4096];

    // select() tells which socket(s) are readable, so we can forward in whichever direction has data, without blocking
    while(true){
        fd_set readSet;
        FD_ZERO(&readSet);
        FD_SET(client_socket, &readSet);
        FD_SET(remote_socket, &readSet);

        struct timeval tv;
        tv.tv_sec  = 30;   // 30 sec connection timeout
        tv.tv_usec = 0;

        int nfds = std::max(client_socket, remote_socket) + 1;
        int ready = select(nfds, &readSet, nullptr, nullptr, &tv);
        if(ready <= 0){
            break;   // 0 = timeout, <0 = select error
        }

        // Client sent data -> forward to origin
        if(FD_ISSET(client_socket, &readSet)){
            int n = recv(client_socket, buffer, sizeof(buffer), 0);
            if(n <= 0){
                break;   // browser closed the connection
            }
            if(send(remote_socket, buffer, n, 0) == -1){
                break;
            }
        }
        // Origin sent data -> forward to client
        if(FD_ISSET(remote_socket, &readSet)){
            int n = recv(remote_socket, buffer, sizeof(buffer), 0);
            if(n <= 0){
                break;   // origin closed the connection
            }
            if(send(client_socket, buffer, n, 0) == -1){
                break;
            }
        }
    }
}

} // namespace miniCDN