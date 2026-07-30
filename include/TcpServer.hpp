#ifndef TCPSERVER_HPP
#define TCPSERVER_HPP

#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h> // Required for TCP_NODELAY
#include <unistd.h>

#include <atomic>

#include "LRUCache.hpp"
#include "Threadpool.hpp"

namespace miniCDN{

class TcpServer{
public:
    TcpServer(int port, int pool_size);
    ~TcpServer();

    void start();
    void stop();

private:
    int m_port;
    int m_server_socket;
    sockaddr_in m_server_addr;

    LRUCache m_cache;
    Threadpool m_threapool;

    std::atomic<bool> m_running{true};

    void setupSocket();
    void handleClient(int client_socket);
    void cleanup();
};

} // namespace miniCDN

#endif