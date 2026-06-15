#include "TcpServer.hpp"
#include "../http/HttpRequest.hpp"
#include "../proxy/ProxyHandler.hpp"

#include <iostream>
#include <stdexcept>
#include <string>

namespace miniCDN{

TcpServer::TcpServer(int port) : m_port(port), m_server_socket(INVALID_SOCKET), m_cache(100), m_threapool(16) {
    setupSocket();
}

TcpServer::~TcpServer(){
    cleanup();
}

void TcpServer::setupSocket(){
    // 1. initialize winsock
    int iResult = WSAStartup(MAKEWORD(2, 2), &m_wsaData);
    if(iResult != 0){
        throw std::runtime_error("WSAStartup failed: " + std::to_string(iResult));
    }

    // 2. create socket
    m_server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if(m_server_socket == INVALID_SOCKET){
        WSACleanup();
        throw std::runtime_error("Error at socket():" + std::to_string(WSAGetLastError()));
    }

    // 3. Bind the socket
    m_server_addr.sin_family = AF_INET;
    m_server_addr.sin_addr.s_addr = INADDR_ANY;
    m_server_addr.sin_port = htons(m_port);

    // if(InetPton(AF_INET, _T("0.0.0.0"), &m_server_addr.sin_addr) != 1){
    //     cleanup();
    //     throw std::runtime_error("Error at InetPton():" + std::to_string(WSAGetLastError()));
    // } 

    if(bind(m_server_socket, (SOCKADDR*)&m_server_addr, sizeof(m_server_addr)) == SOCKET_ERROR){
        cleanup();
        throw std::runtime_error("bind failed with error:" + std::to_string(WSAGetLastError()));
    }
    
    // 4. listen
    if(listen(m_server_socket, SOMAXCONN) == SOCKET_ERROR){
        cleanup();
        throw std::runtime_error("listen failed with error:" + std::to_string(WSAGetLastError()));
    }

    std::cout << "[server] Winsock initialized. Listening on Port " << m_port << "...\n";
}

void TcpServer::start(){
    while(m_running){
        SOCKET client_socket = INVALID_SOCKET;
        sockaddr_in client_addr;
        int client_len = sizeof(client_addr);

        std::cout << "\n[server] Waiting for connection...\n";

        client_socket = accept(m_server_socket, (struct sockaddr*)&client_addr, &client_len);
        if(client_socket == INVALID_SOCKET){
            // if stop() was called, socket was closed deliberately. break
            if(!m_running){
                break;
            }

            std::cerr << "[Error] accept failed: " << WSAGetLastError() << "\n";
            continue;
        }

        // Set a timeout for the client socket, which doesn't send data or is slow to sned
        // SO_RCVTIMEO make recv() return WSAETIMEDOUT after 5 seconds, so handleClient() can detect and close the connection
        DWORD timeout = 5000;
        setsockopt(client_socket, SOL_SOCKET, SO_RCVTIMEO, (const char*)&timeout, sizeof(timeout));
            
        std::cout << "[server] connection accepted!\n";

        // enqueue the job as a lambda function to threadpool
        m_threapool.enqueue([this, client_socket](){
            this->handleClient(client_socket);
        });
    }
}

void TcpServer::handleClient(SOCKET client_socket){
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
            closesocket(client_socket);
            return;
        }
        else{
            // socket error
            // either WSAETIMEDOUT or normal
            int error = WSAGetLastError();
            if(error == WSAETIMEDOUT){
                std::cerr << "[Server] Client timed out- closing connection\n";
            }
            else{
                std::cerr << "[Server] recv() failed: " << error << "\n";
            }

            closesocket(client_socket);
            return;
        }
    }
    
    HttpRequest request;
    if(request.parse(rawRequest)){
        if(request.getMethod() == "GET"){  // HTTP Request
            ProxyHandler proxy(m_cache);              // pass cache to proxy module
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
    
    closesocket(client_socket);
}

void TcpServer::cleanup(){
    if(m_server_socket != INVALID_SOCKET){
        closesocket(m_server_socket);
        m_server_socket = INVALID_SOCKET;
    }

    WSACleanup();
    std::cout << "[Server] winsock cleaned up.\n";
}

void TcpServer::stop(){
    m_running = false;
    if(m_server_socket != INVALID_SOCKET){
        // Force blocking accept() to return immediately
        closesocket(m_server_socket);

        // Set to INVALID -> cleanup() will not try closing again
        m_server_socket = INVALID_SOCKET;
    }
}

} // namespace miniCDN