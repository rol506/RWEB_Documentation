#pragma once

#include <string>

#ifdef __linux__
#include <netinet/in.h>
#include <netdb.h>
#elif _WIN32
#undef UNICODE
#define WIN32_LEAN_AND_MEAN
#pragma comment (lib, "Ws2_32.lib")
#include <windows.h>
#define ENABLE_VIRTUAL_TERMINAL_PROCESSING 0x0004
#define DISABLE_NEWLINE_AUTO_RETURN  0x0008
#endif

#include "Socket.h"
#include "HTMLTemplate.h"
#include "Utility.h"

namespace rweb {

  struct Request 
  {
    std::string method;
    std::string path;
    std::string protocol;
    std::string headers;
    bool isValid = false;
  }; 

  typedef HTMLTemplate (*HTTPCallback)(const Request r);

  //---FRAMEWORK-INTERNAL---

  static std::string getExecutablePath();
  static std::string calculateResourcePath(size_t level);
  static Request parseRequest(const std::string request);
  static void handleClient(const Request r, const SOCKFD newsockfd);

  //---FRAMEWORK---

  std::string describeError();
  std::string getResourcePath();
  int getPort();
  bool getShouldClose();
  void setShouldClose(bool _shouldClose);
  bool getDebugState();
  void setResourcePath(const std::string resPath);
  void setPort(const int port);
  void addRoute(const std::string& path, const HTTPCallback callback);
  void addResource(const std::string& URLpath, const std::string& resourcePath, const std::string& contentType);
  HTMLTemplate createTemplate(const std::string& templatePath, const std::string& statusResponce);
  std::string sendFile(const std::string& statusResponce, const std::string& filePath, const std::string& contentType);

  //returns false on error.
  bool init(bool debug = false, unsigned int level=0);
  //returns false on an error.
  bool startServer(const int clientQueue);
#ifdef __linux__
  void closeServer(int arg=0);
#elif _WIN32
  BOOL WINAPI closeServer(DWORD signal=CTRL_C_EVENT);
#endif

  //---HTTP RESPONCES---
  const std::string HTTP_200 = "HTTP/1.1 200 OK\r\n";

  const std::string HTTP_400 = "HTTP/1.1 400 Bad Request\r\n";
  const std::string HTTP_401 = "HTTP/1.1 401 Unauthorized\r\n";
  const std::string HTTP_403 = "HTTP/1.1 403 Forbidden\r\n";
  const std::string HTTP_404 = "HTTP/1.1 404 Not Found\r\n";
  const std::string HTTP_405 = "HTTP/1.1 405 Method Not Allowed\r\n";

  const std::string HTTP_500 = "HTTP/1.1 500 Internal Server Error\r\n";
}

