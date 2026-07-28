#include <iostream>
#include <csignal>

#include "TcpServer.hpp"

// Global pointer required for the signal handler to access the server instance
miniCDN::TcpServer* globalServer = nullptr;

void handleShutdown(int signum){
    std::cout << "\n[Interrupt] Caught signal" << signum << ". Graceful shutdown\n";
    if(globalServer != nullptr){
        globalServer->stop();
    }
}

int main(int argc, char* argv[]){
    std::signal(SIGINT, handleShutdown);

    int pool_size = (argc > 1) ? std::stoi(argv[1]) : 16;

    try{
        miniCDN::TcpServer server(8080, pool_size);
        globalServer = &server;
        
        std::signal(SIGPIPE, SIG_IGN);
        server.start();
    }
    catch(const std::exception &e){
        std::cerr << "[Fatal Error] " << e.what() << "\n";
        return 1;
    }
    return 0;
}