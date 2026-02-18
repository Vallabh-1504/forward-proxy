#include "../server/TcpServer.hpp"
#include <iostream>

int main(){
    try{
        miniCDN::TcpServer server(8080);
        server.start();
    }
    catch(const std::exception &e){
        std::cerr << "[Fatal Error] " << e.what() << "\n";
        return 1;
    }
    return 0;
}