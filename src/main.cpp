#include <iostream>
#include <csignal>
#include <string>
#include <algorithm>

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

    int port = (argc > 1) ? std::stoi(argv[1]) : 8080;
    int pool_size = (argc > 2) ? std::stoi(argv[2]) : 16;

    // remaining args for --no-rate-limit flag
    bool rate_limiting = true;
    for (int i = 3; i < argc; ++i) {
        if (std::string(argv[i]) == "--no-rate-limit") {
            rate_limiting = false;
            break;
        }
    }

    std::cout << "[Server] Port=" << port << "  Threads=" << pool_size << "  RateLimit=" << (rate_limiting ? "ON" : "OFF") << "\n";

    try{
        miniCDN::TcpServer server(port, pool_size, rate_limiting);
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