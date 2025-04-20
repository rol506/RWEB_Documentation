#include <RWEB.h>

#include <iostream>
#include <random>
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

//TODO add math support in templates

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

  std::string getFileString(const std::string& filePath)
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

  //remove leading and trailing spaces
  std::string trim(const std::string& str)
  {
    static const std::string ws = " \t\n";
    auto first = str.find_first_not_of(ws);
    auto last = str.find_last_not_of(ws);
    return first == std::string::npos ? "" : str.substr(first, last-first+1);
  }

  std::vector<std::string> split(const std::string& s, char seperator)
  {
    std::vector<std::string> output;

    std::string::size_type prev_pos = 0, pos = 0;

    while((pos = s.find(seperator, pos)) != std::string::npos)
    {
      std::string substring(trim(s.substr(prev_pos, pos-prev_pos)));
      if (substring.size() > 0)
      {
        output.push_back(substring);
      }
      prev_pos = ++pos;
    }
    output.push_back(trim(s.substr(prev_pos, pos-prev_pos))); // Last word
    return output;
  }

  //multiple seperators
  std::vector<std::string> split(const std::string& s, const std::string& seperator)
  {
    std::vector<std::string> output;
    std::string::size_type prev_pos = 0, pos = 0;

    while ((pos = s.find_first_of(seperator, pos)) != std::string::npos)
    {
      std::string substring(trim(s.substr(prev_pos, pos-prev_pos)));
      if (substring.size() > 0)
      {
        output.push_back(substring);
      }
      prev_pos = ++pos;
    }
    output.push_back(trim(s.substr(prev_pos, pos-prev_pos))); //last word
    return output;
  }

  std::string getOperator(const std::string& s)
  {
    auto pos1 = s.find("{%")+2;
    auto pos2 = s.substr(pos1).find("%}");
    if (pos1 == std::string::npos || pos2 == std::string::npos)
    {
      return "";
    }
    return s.substr(pos1, pos2);
  }

  std::string replace(const std::string& s, const std::string& from, const std::string& to) noexcept
  {
    std::string cpy = s.substr();
    size_t pos = cpy.find(from);
    while (pos != std::string::npos)
    {
      cpy.replace(pos, from.size(), to);
      if (pos + from.size() >= cpy.size())
      {
        break;
      }

      pos = cpy.find(from, pos+from.size());
    }
    return cpy;
  } 

  double calculate(const std::string& expression)
  {
    std::string expr = replace(expression, " ", ""); //remove spaces

    while (expr.find_first_of("()") != std::string::npos)
    {
      auto pos1 = expr.find_last_of("(");
      auto pos2 = expr.find_first_of(")", (pos1 == std::string::npos ? 0 : pos1));

      if (pos1 == std::string::npos)
      {
        std::cerr << "[CALC] ERROR FOUND UNOPENED BRACKET: " << pos2 << "\n";
        return 0;
      }

      if (pos2 == std::string::npos)
      {
        std::cerr << "[CALC] ERROR COUND UNCLOSED BRACKET!\n";
        return 0;
      }

      std::string brackets = expr.substr(pos1+1, pos2-pos1-1);
      double res = calculate(brackets);
      expr.replace(pos1, pos2-pos1+1, std::to_string(res));
    }

    //without brackets
    while (expr.find("**") != std::string::npos)
    {
      auto before1 = expr.substr(0, expr.find("**")).find_last_not_of("*"); 
      auto before2 = expr.find_first_of("*/+-", before1+3); //+3 because ** is 2 length and the next char

      std::string tmp = expr.substr(before1, before2-before1);
      auto vec = split(tmp, "*");
      expr = replace(expr, tmp, std::to_string((pow(atof(vec[0].c_str()), atof(vec[1].c_str())))));
    } 

    std::size_t pos1=0, pos2=0, pos3=0;
    while (expr.find_first_of("* /") != std::string::npos)
    {
      while (pos2 != std::string::npos)
      {
        pos2 = expr.find_first_of("*+-/", pos1+1);
        if (pos2 == std::string::npos)
        {
          break;
        }
        if (expr[pos2] == '*' || expr[pos2] == '/')
        {
          pos3 = expr.find_first_of("*+-/", pos2+1);  
          if (expr[pos2] == '*')
          {
            double res = atof(expr.substr(pos1, pos2-pos1).c_str()) * atof(expr.substr(pos2+1, pos3-pos2-1).c_str());
            expr = replace(expr, expr.substr(pos1, pos2-pos1) + expr[pos2] + expr.substr(pos2+1, pos3-pos2-1), std::to_string(res));
            continue;
          } else {
            double res = atof(expr.substr(pos1, pos2-pos1).c_str()) / atof(expr.substr(pos2+1, pos3-pos2-1).c_str());
            expr = replace(expr, expr.substr(pos1, pos2-pos1) + expr[pos2] + expr.substr(pos2+1, pos3-pos2-1), std::to_string(res));
            continue;
          }
        } else {
          pos1 = pos2+1;
          pos2 = expr.find_first_of("*+-/", pos1+1);
          continue;
        }
      }
    }

    pos1=0; pos2=0; pos3=0;
    while (expr.find_first_of("+-") != std::string::npos)
    {
      while (pos2 != std::string::npos)
      {
        pos2 = expr.find_first_of("+-", pos1+1);
        if (pos2 == std::string::npos)
        {
          break;
        }
        if (expr[pos2] == '+' || expr[pos2] == '-')
        {
          pos3 = expr.find_first_of("+-", pos2+1);  
          if (expr[pos2] == '+')
          {
            double res = atof(expr.substr(pos1, pos2-pos1).c_str()) + atof(expr.substr(pos2+1, pos3-pos2-1).c_str());
            expr = replace(expr, expr.substr(pos1, pos2-pos1) + expr[pos2] + expr.substr(pos2+1, pos3-pos2-1), std::to_string(res));
            continue;
          } else {
            double res = atof(expr.substr(pos1, pos2-pos1).c_str()) - atof(expr.substr(pos2+1, pos3-pos2-1).c_str());
            expr = replace(expr, expr.substr(pos1, pos2-pos1) + expr[pos2] + expr.substr(pos2+1, pos3-pos2-1), std::to_string(res));
            continue;
          }
        } else {
          pos1 = pos2+1;
          pos2 = expr.find_first_of("*+-/", pos1+1);
          continue;
        }
      }
    }
    return atof(expr.c_str());
  }

  void HTMLTemplate::renderJSON(const nlohmann::json& json)
  {
    std::string html = m_html;
    size_t sz = html.size();

    int currLine = 1;
    int currChar = 1;

    for (int i=0;i<sz;++i)
    {
      char c = m_html[i];

      if (c == '\n')
      {
        currChar = 1;
        currLine++;
      } else {
        currChar++;
      }

      if (c == '{' && m_html[i+1] == '%') //if/for 
      {
        size_t startPos = i;
        std::string condition;
        std::string body;
        size_t len = 0;
        std::istringstream ss(m_html.substr(i+2));
        std::getline(ss, condition, '%');
        len = condition.size();
        condition = trim(condition);
        //std::cout << "[TEMPLATE] Found condition: " << '"' << condition << '"' << "\n";

        auto elements = split(condition, ' '); 

        if (elements[0] == "for")
        {
          if (elements.size() == 4)
          {
            std::string endfor = getOperator(m_html.substr(m_html.find(condition)));
            while (trim(endfor) != "endfor" && trim(endfor) != "")
            {
              endfor = getOperator(m_html.substr(m_html.find(endfor)));
            }

            if (trim(endfor) == "endfor" && endfor != "")
            {
              size_t pos1 = m_html.find(condition)+len+1;
              size_t pos2 = m_html.find("{%" + endfor + "%}");
              body = trim(m_html.substr(pos1, pos2-pos1));
              std::string res = "";

              if (elements[2] != "in")
              {
                std::cerr << "[TEMPLATE] Bad for loop syntax!\n";

                html.replace(pos2, endfor.size()+4, "");
                html.replace(pos1-len-2, len+4, "");

                i = 0;
                m_html = html;
                currLine = 1;
                currChar = 1;
                continue;
              }

              std::string result = "";
              if (elements[3].substr(0, 5) == "range")
              {
                std::string count = elements[3].substr(6, elements[3].size()-6-1);
                int cnt = atoi(count.c_str());
                if (!cnt)
                {
                  std::cerr << "[TEMPLATE] Error: range only supports int argument > 0!\n";
                } else {
                  for (int k=0;k<cnt;++k)
                  {
                    result += replace(body, "{{" + elements[1] + "}}", std::to_string(k));
                  }
                }

              } else {
                //iterating dict
                std::string& name = elements[3];
                
                auto value = json.find(name);
                if (value != json.end())
                {
                  if (value->is_array())
                  {
                    result = "";
                    for (auto it = value->begin(); it != value->end();++it)
                    { 
                      std::string tmp = body;
                      size_t pos1 = 0;
                      do {
                        pos1 = tmp.find("{{")+2;
                        size_t pos2 = tmp.find("}}");
                        if (pos1 == std::string::npos || pos2 == std::string::npos)
                        {
                          break;
                        }

                        std::string name = tmp.substr(pos1, pos2-pos1);
                        std::vector<std::string> subscripts = split(name, '.');

                        auto val = it->find(subscripts[1]);
                        if (val == it->end())
                        {
                          std::cout << "[TEMPLATE] Can't find value of: " << subscripts[1] << "\n";
                          break;
                        }
                        auto res = val;
                        if (subscripts.size() > 2)
                        {
                          for (int i=1;i<subscripts.size();++i)
                          {
                            nlohmann::json::const_iterator tmp = val->find(subscripts[i]);
                            if (tmp != val->end())
                            {
                              val = tmp;
                            } else {
                              std::cout << "[TEMPLATE] Can't find value of " << subscripts[i] << "\n";
                              break;
                            }
                          }
                        }

                        res = val;
                        tmp.replace(pos1-2, pos2-pos1+4, (std::string)*res);

                      } while (pos1 != std::string::npos);
                      result += tmp;
                    }
                  } else {
                    if (value->is_object())
                    {
                      std::cerr << "[TEMPLATE] Iterating dicts is an unsupported option! Value of " << '"' << name << '"' << " is an object!\n";
                    } else {
                      std::cerr << "[TEMPLATE] Value of " << '"' << name << '"' << " is not a json dict or an array!\n";
                    }
                  }
                } else {
                  std::cerr << "[TEMPLATE] Could not find value with key " << '"' << name << '"' << "\n";
                }
              }

              html.replace(startPos, m_html.find("{%" + endfor + "%}", startPos)-startPos+endfor.size()+4, result);

              i = 0;
              m_html = html;
              sz = m_html.size();
              currLine = 1;
              currChar = 1;
              continue;
            } else {
              std::cerr << "[TEMPLATE] Error: for loop must end with {% endfor %}!\n";
              html.replace(startPos, m_html.find("{%" + endfor + "%}", startPos)-startPos+endfor.size()+4, "");
              i = 0;
              m_html = html;
              sz = m_html.size();
              currLine = 1;
              currChar = 1;
              continue;
            }
          }
          
          std::cerr << "[TEMPLATE] Error: invalid for loop syntax!\n";
          html.replace(i, len+4, ""); 
        } else if (elements[0] == "if")
        {
          return;
        } else {
          std::cerr << "[TEMPLATE] Error: unknown operator " << '"' << elements[0] << '"' << "\n";
          std::cout << m_html << "\n";
          html.replace(i, len+4, "");
        }

        m_html = html;
        i = 0;
        sz = m_html.size();
        currLine = 1;
        currChar = 1;

      } else if (c == '{' && m_html[i+1] == '{') //variable
      {
        std::string name;
        size_t len = 0;
        std::istringstream ss(m_html.substr(i+2));
        std::getline(ss, name, '}');
        //len must be calculated before trimming
        len = name.size();
        name = trim(name);
        //std::cout << "[TEMPLATE] Found variable: " << '"' << name << '"' << "\n";

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
        currLine = 1;
        currChar = 1;
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
