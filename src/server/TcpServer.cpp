#include "TcpServer.hpp"
#include "../http/HttpRequest.hpp"
#include <iostream>
#include <stdexcept>

namespace miniCDN{

TcpServer::TcpServer(int port) : m_port(port), m_server_socket(INVALID_SOCKET){
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
    while(true){
        SOCKET client_socket = INVALID_SOCKET;
        sockaddr_in client_addr;
        int client_len = sizeof(client_addr);

        std::cout << "[server] Waiting for connection...\n";

        client_socket = accept(m_server_socket, (struct sockaddr*)&client_addr, &client_len);
        if(client_socket == SOCKET_ERROR){
            std::cerr << "[Error] accept failed: " << WSAGetLastError() << "\n";
            continue;
        }

        std::cout << "[server] connection accepted!\n";
        handleClient(client_socket);
    }
}

void TcpServer::handleClient(SOCKET client_socket){
    char buffer[4096];
    int bytes_read = recv(client_socket, buffer, sizeof(buffer), 0);

    if(bytes_read > 0){
        if(bytes_read < 4096) buffer[bytes_read] = '\0';
        std::cout << "[client request]:\n" << buffer << "\n";

        // ---- Parsing logic ---
        std::string rawRequest(buffer);
        HttpRequest request;

        if(request.parse(rawRequest)){
            std::cout << "[Server] request parsed successfully!\n";
            request.printInfo();
        }
        else{
            std::cout << "[server] Failed to parse request.\n";
        }
        // ----------------------

        std::string response = "HTTP/1.1 200 OK\r\nContent-Type: HTTP\r\n\r\n<H1>Hello from miniCDN!</H1>";

        int iSendResult = send(client_socket, response.c_str(), (int)response.length(), 0);
        if(iSendResult == SOCKET_ERROR){
            std::cerr << "send failed: " << WSAGetLastError() << "\n";
        }
    }
    else if(bytes_read == 0){
        std::cout << "Connection Closing...\n";
    }
    else{
        std::cerr << "recv failed: " << WSAGetLastError() << "\n";
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

} // namespace miniCDN