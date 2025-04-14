#include <RWEB.h>

#include <iostream>
#include <string>
#include <unordered_map>
#include <errno.h>
#include <thread>
#include <fstream>
#include <sstream>

#ifdef __linux__
#include <unistd.h>
#include <signal.h>

#include <sys/types.h>
#include <sys/socket.h>
#elif _WIN32
#include <ws2tcpip.h>
#endif

namespace rweb
{
  static std::string resourcePath = "";
  static std::unordered_map<std::string, HTTPCallback> serverPaths;
  static std::unordered_map<std::string, std::pair<std::string, std::string>> serverResources;
  static int serverPort = 4221;
  static bool serverDebugMode = false;
  static bool serverCompression = false; //not supported yet
  static bool shouldClose = false;
  //static int sockfd; //server socket file descriptor
  static bool initialized = false;
  static std::shared_ptr<Socket> serverSocket;

  //use in case if you are running app in build folder, but editing source code folder (will step back <level> times for res folder)
  static int sourcePathLevel = 0; //set only at compile time for safety 

  Socket::Socket(int clientQueue)
    : m_debug(false), m_connected(false)
  {
#ifdef __linux__

    struct sockaddr_in serv_addr;

    m_socket = SOCKFD{ 0 };

    m_socket.sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (m_socket.sockfd < 0)
    {
      std::cerr << "[ERROR] Can't open socket!\n";
      shouldClose = true;
      return;
    }

    const int enable = 1;
    if (setsockopt(m_socket.sockfd, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int)) < 0)
    {
      std::cerr << "[ERROR] setsockopt failed! (SO_REUSEADDR)\n";
      shouldClose = true;
      return;
    }

    bzero(&serv_addr, sizeof(serv_addr));

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = INADDR_ANY;
    serv_addr.sin_port = htons(serverPort);

    if (bind(m_socket.sockfd, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) < 0)
    {
      std::cerr << "[ERROR] Failed to bind socket: " << describeError() << "\n";
      close(m_socket.sockfd);
      shouldClose = true;
      return;
    }

    listen(m_socket.sockfd, clientQueue); 

#elif _WIN32

    m_result = NULL;

    m_socket = SOCKFD{INVALID_SOCKET};

    int iResult = WSAStartup(MAKEWORD(2, 2), &m_wsaData);
    if (iResult != 0)
    {
      std::cerr << "[ERROR] WSAStartup failed with error: " << iResult << "\n";
      std::cerr << "[ERROR] Can't open socket!\n";
      shouldClose = true;
    }

    ZeroMemory(&m_hints, sizeof(m_hints));
    m_hints.ai_family = AF_UNSPEC;
    m_hints.ai_socktype = SOCK_STREAM;
    m_hints.ai_protocol = IPPROTO_TCP;

    // Resolve the server address and port
    iResult = getaddrinfo(NULL, std::to_string(serverPort).c_str(), &m_hints, &m_result);
    if ( iResult != 0 ) {
      std::cerr << "[ERROR] getaddrinfo failed with error: " << iResult << "\n";
      WSACleanup();
      shouldClose = true;
      return;
    }

    // Create a SOCKET for the server to listen for client connections.
    m_socket.sockfd = socket(m_result->ai_family, m_result->ai_socktype, m_result->ai_protocol);
    if (m_socket.sockfd == INVALID_SOCKET) {
      std::cerr << "[ERROR] socket failed with error: " << WSAGetLastError() << "\n";
      freeaddrinfo(m_result);
      WSACleanup();
      shouldClose = true;
      return;
    }

    // Setup the TCP listening socket
    iResult = bind( m_socket.sockfd, m_result->ai_addr, (int)m_result->ai_addrlen);
    if (iResult == SOCKET_ERROR) {
      std::cerr << "[ERROR] bind failed with error: " << WSAGetLastError() << "\n";
      freeaddrinfo(m_result);
      closesocket(m_socket.sockfd);
      WSACleanup();
      shouldClose = true;
      return;
    }

    freeaddrinfo(m_result);

    iResult = listen(m_socket.sockfd, clientQueue);
    if (iResult == SOCKET_ERROR) {
      std::cerr << "[ERROR] listen failed with error: " << WSAGetLastError() << "\n";
      closesocket(m_socket.sockfd);
      WSACleanup();
      shouldClose = true;
      return;
    }
#endif
  }

  void Socket::closeSocket(SOCKFD socket)
  {
#ifdef __linux__

    shutdown(socket.sockfd, SHUT_RDWR);
    close(socket.sockfd);

#elif _WIN32

    closesocket(socket.sockfd);

#endif
  }

  Socket::~Socket()
  {

    m_connected = false;
    shouldClose = true;

#ifdef __linux__

    bool err = false;

    if (shutdown(m_socket.sockfd, 0) && errno != ENOTCONN)
    {
      std::cerr << "[ERROR] Failed to shutdown server socket: " << describeError() << "\n";
      err = true;
    }
    if (::close(m_socket.sockfd))
    {
      std::cerr << "[ERROR] Failed to close server socket: " << describeError() << "\n";
      err = true;
    }

    if (!err)
    {
      std::cout << "[SERVER] Socket was shut down successfully!\n";
    } else {
      std::cerr << "[ERROR] Failed to shutdown socket!\n";
    }

#elif _WIN32
    closesocket(m_socket.sockfd);
    WSACleanup();

    std::cout << "[SERVER] Socket was shut down successfully!\n";
#endif
  }

  /*bool Socket::connect(const std::string& hostname, const int port)
  {
    return false;
  }*/

  bool Socket::sendMessage(SOCKFD clientSocket, const std::string& message)
  {
#ifdef __linux__
    m_count = write(clientSocket.sockfd, message.c_str(), message.size());
    if (m_count < 0)
    {
      if (m_debug)
        std::cerr << "[ERROR] Failed to write to socket!\n";
      return false;
    }

    return true;
#elif _WIN32

    int iResult = send(clientSocket.sockfd, message.c_str(), message.size(), 0);
    if (iResult == SOCKET_ERROR)
    {
      std::cerr << "[ERROR] send failed: " << WSAGetLastError() << "\n";
      return false;
    }

    return true;

#endif
  }

  std::string Socket::getMessage(SOCKFD clientSocket)
  {

  #ifdef __linux__
    int received = 0;

    std::string request(SERVER_BUFLEN, '\0');
    do {
      int n = read(clientSocket.sockfd, &request[0], SERVER_BUFLEN-1);
      if (n < 0)
      {
        shutdown(clientSocket.sockfd, SHUT_RDWR);
        close(clientSocket.sockfd);
        if (shouldClose)
          return request;

        std::cerr << "[ERROR] Failed to read from client socket: " << describeError() << "\n";
        return request;
      }
      if (n == 0 || request.find("\r\n\r\n") != std::string::npos)
      {
        break;
      }
      received += n;
    } while (received < SERVER_BUFLEN-1);

    return request;

#elif _WIN32

    std::string request(SERVER_BUFLEN, '\0');
    char* buffer = (char*)malloc(SERVER_BUFLEN);
    int received = 0;

    int iResult = 0;

    do {
      iResult = recv(clientSocket.sockfd, buffer, SERVER_BUFLEN, 0);
      request += std::string(buffer);
      received += iResult;

      if (iResult == 0 || request.find("\r\n\r\n") != std::string::npos)
      {
        break;
      } else if (iResult < 0)
      {
        std::cerr << "[ERROR] recv failed: " << WSAGetLastError() << "\n";
      }
    } while (iResult > 0);

    free(buffer);
    return request;
#endif
  }

  SOCKFD Socket::acceptClient()
  {
#ifdef __linux__

    struct sockaddr_in cli_addr;

    socklen_t cli_len = sizeof(cli_addr);
    int newsockfd = ::accept(m_socket.sockfd, (struct sockaddr*)&cli_addr, &cli_len);

    return {newsockfd};

#elif _WIN32

    SOCKET ClientSocket = ::accept(m_socket.sockfd, NULL, NULL);
    if (ClientSocket == INVALID_SOCKET && !shouldClose) {
      std::cerr << "[ERROR] accept failed with error: " << WSAGetLastError() << "\n";
      closesocket(m_socket.sockfd);
      WSACleanup();
    }

    return SOCKFD{ClientSocket};

#endif
  }

  static std::string getExecutablePath()
  {
#ifdef __linux__
    char* path = (char*)malloc(1024);
    size_t r;
    r = readlink("/proc/self/exe", path, 1023);
    std::string execPath = path;
    free(path);
    return execPath;
#elif _WIN32
    char* path = (char*)malloc(1024);
    GetModuleFileName(NULL, path, 1024);
    std::string execPath = path;
    free(path);
    return execPath;
#endif
  }

  //see sourcePathLevel docs
  static std::string calculateResourcePath(size_t level)
  {
    std::string resPath = getExecutablePath();
#ifdef _WIN32
    level++; //on windows we have "Debug" folder -> increment level by 1
#endif
    for (int i=0;i<=level;++i)
    {
      std::size_t pos = resPath.find_last_of("/\\");
      resPath = resPath.substr(0, pos);
    }
    resPath += "/res";
    return resPath;
  }

  std::string describeError()
  {
    switch (errno)
    {
      case 0:
        return "Success";
      case 9:
        return "Bad file descriptor";
      case 13:
        return "Permission denied";
      case 14:
        return "Bad address";

      case 32:
        return "Broken pipe";
      case 94:
        return "Socket type not supported";
      case 96:
        return "Protocol family not supported";
      case 98:
        return "Address already in use";
      case 100:
        return "Network is down";
      case 101:
        return "Network is unreachable";
      case 104:
        return "Connection reset by peer";
      case 110:
        return "Connection timed out";
      case 111:
        return "Connection refused";

      case ENOTSOCK:
        return "File descriptor is not a socket";
    }
    return std::to_string(errno);
  }

  static Request parseRequest(const std::string request)
  {
    Request r;
    std::string str = request;
    std::size_t pos;

    //method
    pos = str.find(" ");
    r.method = str.substr(0, pos);
    str = str.substr(pos+1);

    //path
    pos = str.find(" ");
    r.path = str.substr(0, pos);
    str = str.substr(pos+1);

    //protocol
    pos = str.find("\r\n");
    if (pos == std::string::npos)
    {
      r.isValid = false;
      return r;
    }
    r.protocol = str.substr(0, pos);
    str = str.substr(pos+1);

    r.isValid = true;
    return r;
  }

  std::string getResourcePath()
  {
    return resourcePath;
  }

  void setResourcePath(const std::string resPath)
  {
    resourcePath = resPath;
  }

  void addRoute(const std::string& path, const HTTPCallback callback)
  {
    serverPaths.emplace(path, callback);
  }

  void addResource(const std::string& URLpath, const std::string& resourcePath, const std::string& contentType)
  {
    serverResources.insert({URLpath, {resourcePath, contentType}});
  }

  void setPort(const int port)
  {
    serverPort = port;
  }

  int getPort()
  {
    return serverPort;
  }

  bool getDebugState()
  {
    return serverDebugMode;
  }

  //returns false on an error (step <level> times back to find resource folder. use only for dev purposes)
  bool init(bool debug, unsigned int level)
  {
    serverDebugMode = debug;

#ifdef __linux
    signal(SIGINT, closeServer);
    signal(SIGTERM, closeServer);
#elif _WIN32
    if (!SetConsoleCtrlHandler(closeServer, TRUE))
    {
      std::cerr << "[ERROR] Failed to create Ctrl C handler!\n";
      return false;
    }
#endif
    //resource path
    setResourcePath(calculateResourcePath(level));
    initialized = true;

    return true;
  }

  //returns false on an error
  bool startServer(const int clientQueue)
  {

    serverSocket = std::make_shared<Socket>(clientQueue);

    while (!shouldClose)
    {
      SOCKFD newSock = serverSocket->acceptClient();

      if (shouldClose)
      {
        break;
      }
      std::string request = serverSocket->getMessage(newSock);

      Request req = parseRequest(request);

      std::thread th(handleClient, req, newSock);
      th.detach();
    }

    return true;
  }

  static void handleClient(const Request r, const SOCKFD newsockfd)
  {
    std::string res;
    int n = 0;

    if (!r.isValid)
    {
      res = HTTP_400 + "\r\n";
      std::cout << "[RESPONCE] " << r.path << " -- " << HTTP_400;
      serverSocket->sendMessage(newsockfd, res);
      Socket::closeSocket(newsockfd);
      return;
    }
  
    //process request
    auto it = serverPaths.find(r.path);
    if (it == serverPaths.end())
    {
      auto it2 = serverResources.find(r.path);
      if (it2 != serverResources.end())
      {
        res = sendFile(HTTP_200, it2->second.first, it2->second.second);
        std::cout << "[RESPONCE] " << r.path << " -- " << HTTP_200;
      } else { 
        res = HTTP_404 + "\r\n";
        std::cout << "[RESPONCE] " << r.path << " -- " << HTTP_404;
      }
    } else {
      HTMLTemplate temp = it->second(r);
      res = temp.getStatusResponce() + "Content-Type: " + temp.getContentType() + "\r\nContent-Length: " + std::to_string(temp.getHTML().size()) + "\r\n\r\n";
      res += temp.getHTML();
      std::cout << "[RESPONCE] " << r.path << " -- " << temp.getStatusResponce().substr(9);
    }

    //send result
    serverSocket->sendMessage(newsockfd, res);
    Socket::closeSocket(newsockfd);
    return; 
  }

#ifdef __linux__
  void closeServer(int arg)
  {
    //linux handler
    std::cout << "\n";
    shouldClose = true; 
    serverSocket = nullptr;
  }
#elif _WIN32

  BOOL WINAPI closeServer(DWORD signal)
  {
    //windows handler
    if (signal == CTRL_C_EVENT)
    {
      std::cout << "\n";
      shouldClose = true;
      serverSocket = nullptr;
    }
    return TRUE;
  }

#endif

  static std::string getFileString(const std::string& filePath)
  {
    std::ifstream f;
    f.open(resourcePath + "/" + filePath, std::ios::in | std::ios::binary);
    if (!f.is_open())
    {
      return std::string{};
    }

    std::stringstream buf;
    buf << f.rdbuf();
    return buf.str();
  }

  std::string sendFile(const std::string& statusResponce, const std::string& filePath, const std::string& contentType)
  {
    std::string file = getFileString(filePath);
    if (file.empty())
    {
      return HTTP_500 + "\r\n";
    }
    return statusResponce + "Content-Type: " + contentType + "\r\nContent-Length: " + std::to_string(file.size()) + "\r\nContent-Encoding: utf-8\r\n"
      + "\r\n" + file;
  }

  HTMLTemplate createTemplate(const std::string& templatePath, const std::string& statusResponce)
  {
    std::string file = getFileString(templatePath);
    std::string resp = statusResponce;
    if (file.empty())
    {
      std::cerr << "[TEMPLATE] File " << templatePath << " is empty!\n";
      resp = HTTP_500;
    }

    HTMLTemplate temp(file);
    temp.m_contentType = "text/html";
    temp.m_encoding = "utf-8";
    temp.m_responce = resp;
    temp.m_templateFileName = templatePath;
    return temp;
  }

  HTMLTemplate::HTMLTemplate(const std::string& html)
    : m_html(html)
  {
  }

  void HTMLTemplate::renderJSON(const nlohmann::json& json)
  {
    std::string html = m_html;
    size_t sz = html.size();

    int currLine = 0;
    int currChar = 0;

    for (int i=0;i<sz;++i)
    {
      char c = m_html[i];

      if (c == '\n')
      {
        currChar = 0;
        currLine++;
      } else {
        currChar++;
      }

      if (c == '{' && m_html[i+1] == '%') //if or for 
      {
        std::cout << "[TEMPLATE] Template error (" << currLine << ";" << currChar << "): conditions and loops are unsupported yet!\n";
        return;
      } else if (c == '{' && m_html[i+1] == '{') //variable
      {
        std::string name;
        size_t len = 0;
        for (int j=2;j+i<sz;++j)
        {
          char c = m_html[j+i];
          if (c == '}')
          {
            if (m_html[j+i+1] != '}') //no second closing '}'
            {
              std::cout << "[TEMPLATE] Rendering error (" << currLine << ";" << currChar << "): key names must end with '}}'\n";
              return;
            }

            name = m_html.substr(i+2, j-2);
            len = j-2;
            break;
          }
        }
        //std::cout << "[TEMPLATE] Found value name: " << name << "\n";

        auto value = json.find(name);
        if (value != json.end())
        {
          //std::cout << "[TEMPLATE] Found value: " << *value << "\n";

          if (value->is_string())
          {
            html.replace(i, len+4, (std::string)*value);
          } else if (value->is_number_integer() || value->is_number_unsigned())
          {
            html.replace(i, len+4, std::to_string((long long)*value));
          } else if (value->is_number_float())
          {
            std::stringstream ss;
            ss << (float)*value;
            html.replace(i, len+4, ss.str());
          } else if (value->is_array())
          {
            std::cerr << "[TEMPLATE] Failed to set array value for " << name << "\n";
            html.replace(i, len+4, "");
          }
          else {
            std::cerr << "[TEMPLATE] Value of " << name << " is unsupported type!\n";
            html.replace(i, len+4, "");
          }

        } else {
          std::cout << "[TEMPLATE] Not found value with key: " << name << "\n";
          html.replace(i, len+4, "");
        }
        m_html = html;
        i = 0;
        sz = m_html.size();
      }
    }
  }

  const std::string& HTMLTemplate::getHTML() const
  {
    return m_html;
  }

  const std::string& HTMLTemplate::getFileName() const
  {
    return m_templateFileName;
  }

  const std::string& HTMLTemplate::getStatusResponce() const
  {
    return m_responce;
  }

  const std::string& HTMLTemplate::getEncoding() const
  {
    return m_encoding;
  }

  const std::string& HTMLTemplate::getContentType() const
  {
    return m_contentType;
  }
}
