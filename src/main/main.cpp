#include "../server/TcpServer.hpp"
#include <iostream>
#include <csignal>

// Global pointer required for the signal handler to access the server instance
miniCDN::TcpServer* globalServer = nullptr;

void handleShutdown(int signum){
    std::cout << "\n[Interrupt] Caught signal" << signum << ". Graceful shutdown\n";
    if(globalServer != nullptr){
        globalServer->stop();
    }
}

int main(){
    std::signal(SIGINT, handleShutdown);

    try{
        miniCDN::TcpServer server(8080);
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