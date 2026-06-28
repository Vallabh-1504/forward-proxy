#include "TcpServer.hpp"
#include "../http/HttpRequest.hpp"
#include "../proxy/ProxyHandler.hpp"

#include <iostream>
#include <stdexcept>
#include <cstring>

namespace miniCDN{

TcpServer::TcpServer(int port) : m_port(port), m_server_socket(-1), m_cache(100), m_threapool(16) {
    setupSocket();
}

TcpServer::~TcpServer(){
    cleanup();
}

void TcpServer::setupSocket(){
    // 1. create socket
    m_server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if(m_server_socket == -1){
        throw std::runtime_error("Error at socket():" + std::string(strerror(errno)));
    }

    // 2. Bind the socket
    m_server_addr.sin_family = AF_INET;
    m_server_addr.sin_addr.s_addr = INADDR_ANY;
    m_server_addr.sin_port = htons(m_port);

    if(bind(m_server_socket, (struct sockaddr*)&m_server_addr, sizeof(m_server_addr)) == -1){
        cleanup();
        throw std::runtime_error("bind failed with error:" + std::string(strerror(errno)));
    }
    
    // 3. listen
    if(listen(m_server_socket, SOMAXCONN) == -1){
        cleanup();
        throw std::runtime_error("listen failed with error:" + std::string(strerror(errno)));
    }

    std::cout << "[server] Winsock initialized. Listening on Port " << m_port << "...\n";
}

void TcpServer::start(){
    while(m_running){
        int client_socket = -1;
        sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);

        std::cout << "\n[server] Waiting for connection...\n";

        client_socket = accept(m_server_socket, (struct sockaddr*)&client_addr, &client_len);
        if(client_socket == -1){
            // if stop() was called, socket was closed deliberately. break
            if(!m_running){
                break;
            }

            std::cerr << "[Error] accept failed: " << errno << "\n";
            continue;
        }

        // Set a timeout for the client socket, which doesn't send data or is slow to sned
        // SO_RCVTIMEO make recv() return WSAETIMEDOUT after 5 seconds, so handleClient() can detect and close the connection
        struct timeval timeout{5, 0};
        setsockopt(client_socket, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
            
        std::cout << "[server] connection accepted!\n";

        // enqueue the job as a lambda function to threadpool
        m_threapool.enqueue([this, client_socket](){
            this->handleClient(client_socket);
        });
    }
}

void TcpServer::handleClient(int client_socket){
    // store all incoming bytes until full HTTP header has come, loop until \r\n\r\n

    std::string rawRequest;
    char buffer[4096];

    while(true){
        int bytesRead = recv(client_socket, buffer, sizeof(buffer), 0);

        if(bytesRead > 0){
            // accumulate all bytes
            rawRequest.append(buffer, bytesRead);

            if(rawRequest.find("\r\n\r\n") != std::string::npos) break;
        }
        else if(bytesRead == 0){
            // no bytes received, client didn't send any data
            std::cout << "[Server] Client closed connection before sending data.\n";
            close(client_socket);
            return;
        }
        else{
            // socket error
            // either WSAETIMEDOUT or normal
            int error = errno;
            if(error == EAGAIN || error == EWOULDBLOCK){
                std::cerr << "[Server] Client timed out- closing connection\n";
            }
            else{
                std::cerr << "[Server] recv() failed: " << error << "\n";
            }

            close(client_socket);
            return;
        }
    }
    
    HttpRequest request;
    if(request.parse(rawRequest)){
        if(request.getMethod() == "GET"){  // HTTP Request
            ProxyHandler proxy(m_cache); // pass cache to proxy module
            proxy.handleRequest(client_socket, request);
        }
        else if(request.getMethod() == "CONNECT"){ // HTTPS CONNECT request
            ProxyHandler proxy(m_cache);
            proxy.handleConnect(client_socket, request.getHost(), request.getPort());
        }
        else{
            std::cerr << "[Server] Method " << request.getMethod() << " not supported\n";
        }
    }
    else{
        std::cerr << "[Server] failed to parse HTTP request\n";
    }
    
    close(client_socket);
}

void TcpServer::cleanup(){
    if(m_server_socket != -1){
        close(m_server_socket);
        m_server_socket = -1;
    }
}

void TcpServer::stop(){
    m_running = false;
    if(m_server_socket != -1){
        // Force blocking accept() to return immediately
        close(m_server_socket);

        // Set to INVALID -> cleanup() will not try closing again
        m_server_socket = -1;
    }
}

} // namespace miniCDN