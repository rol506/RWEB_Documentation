#pragma once

#include <string>
#include <nlohmann/json.hpp>

#define SERVER_BUFLEN 64512

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
    //bool connect(const std::string& hostname, const int port);
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

  class HTMLTemplate
  {
  public:
    HTMLTemplate(const std::string& html);
    const std::string& getHTML() const;
    const std::string& getFileName() const;
    const std::string& getStatusResponce() const;
    const std::string& getEncoding() const;
    const std::string& getContentType() const;

    void renderJSON(const nlohmann::json& json);
  private:
    std::string m_html;
    std::string m_templateFileName;
    std::string m_responce;
    std::string m_encoding;
    std::string m_contentType;

    friend HTMLTemplate createTemplate(const std::string&, const std::string&);
  };

  struct Request 
  {
    std::string method;
    std::string path;
    std::string protocol;
    std::string headers;
    bool isValid = false;
  };

  typedef HTMLTemplate (*HTTPCallback)(const Request r);

  static std::string getExecutablePath();
  static std::string calculateResourcePath(size_t level);
  static Request parseRequest(const std::string request);
  static void handleClient(const Request r, const SOCKFD newsockfd);

  std::string describeError();
  std::string getResourcePath();
  int getPort();
  bool getDebugState();
  void setResourcePath(const std::string resPath);
  void setPort(const int port);
  void addRoute(const std::string& path, const HTTPCallback callback);
  void addResource(const std::string& URLpath, const std::string& resourcePath, const std::string& contentType);
  HTMLTemplate createTemplate(const std::string& templatePath, const std::string& statusResponce);
  std::string sendFile(const std::string& statusResponce, const std::string& filePath, const std::string& contentType);

  //utility
  std::string getFileString(const std::string& filePath);
  std::string replace(const std::string& s, const std::string& from, const std::string& to) noexcept;
  std::vector<std::string> split(const std::string& s, char seperator);
    //multiple seperators
  std::vector<std::string> split(const std::string& s, const std::string& seperator);
  double calculate(const std::string& expression);
  const char *colorize(int font);

  //returns false on error
  bool init(bool debug = false, unsigned int level=0);
  //returns false on an error
  bool startServer(const int clientQueue);
#ifdef __linux__
  void closeServer(int arg=0);
#elif _WIN32
  BOOL WINAPI closeServer(DWORD signal=CTRL_C_EVENT);
#endif

  enum COLORS {
#ifdef __linux
      NC=-1,
      BLACK,
      RED,
      GREEN,
      YELLOW,
      BLUE,
      MAGENTA,
      CYAN,
      WHITE,
#else _WIN32

    RED = FOREGROUND_RED | FOREGROUND_INTENSITY,
    YELLOW = FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_INTENSITY,
    GREEN = FOREGROUND_GREEN | FOREGROUND_INTENSITY,
    CYAN = FOREGROUND_GREEN | FOREGROUND_BLUE | FOREGROUND_INTENSITY,
    BLUE = FOREGROUND_BLUE | FOREGROUND_INTENSITY,
    MAGENTA = FOREGROUND_BLUE | FOREGROUND_RED | FOREGROUND_INTENSITY,
    NC = FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE | FOREGROUND_INTENSITY,
    GREY = FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE,
    BLACK = FOREGROUND_INTENSITY,

#endif
  };

  //HTTP RESPONCES
  const std::string HTTP_200 = "HTTP/1.1 200 OK\r\n";

  const std::string HTTP_400 = "HTTP/1.1 400 Bad Request\r\n";
  const std::string HTTP_401 = "HTTP/1.1 401 Unauthorized\r\n";
  const std::string HTTP_403 = "HTTP/1.1 403 Forbidden\r\n";
  const std::string HTTP_404 = "HTTP/1.1 404 Not Found\r\n";
  const std::string HTTP_405 = "HTTP/1.1 405 Method Not Allowed\r\n";

  const std::string HTTP_500 = "HTTP/1.1 500 Internal Server Error\r\n";
}

