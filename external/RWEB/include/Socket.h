#pragma once

#include <string>

#ifdef __linux__
#include <netinet/in.h>
#include <netdb.h>
#elif _WIN32
#define WIN32_LEAN_AND_MEAN
#pragma comment (lib, "Ws2_32.lib")
#include <windows.h>
#include <winsock2.h>
#define ENABLE_VIRTUAL_TERMINAL_PROCESSING 0x0004
#define DISABLE_NEWLINE_AUTO_RETURN  0x0008
#endif

#define SERVER_BUFLEN 64512

namespace rweb {

  //cross-platform wrapper for socket
  struct SOCKFD
  {
#ifdef __linux__
    int sockfd;
#elif _WIN32
    SOCKET sockfd;
#endif
  };

  class Socket
  {
  public: 
    Socket(int clientQueue);
    ~Socket();
    SOCKFD acceptClient();
    bool sendMessage(SOCKFD clientSocket, const std::string& message);
    std::string getMessage(SOCKFD clientSocket);
    static void closeSocket(SOCKFD socket);

  private:

#ifdef __linux__
    int m_count;
    sockaddr_in m_serv_addr;
    hostent* m_server;
#elif _WIN32
    WSADATA m_wsaData;
    struct addrinfo *m_result, m_hints;
#endif
    bool m_debug;
    bool m_connected;
    SOCKFD m_socket;
  };


}
