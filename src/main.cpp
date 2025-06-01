#include <iostream>
#include <cstdlib>

#include <RWEB.h>

static rweb::HTMLTemplate homePage(const rweb::Request r);

void atexit_handler();

int main()
{
  if (!rweb::init(true, 1))
  {
    std::cout << "Failed to initialize RWEB!\n";
    return -1;
  }

  const int atex = std::atexit(atexit_handler);
  if (atex)
  {
    std::cout << rweb::colorize(rweb::RED) << "Failed to register atexit!\n" << rweb::colorize(rweb::NC);
    rweb::closeServer();
    return -1;
  }

  rweb::setPort(4221);
  rweb::setProfilingMode(false);

  rweb::addRoute("/", [](const rweb::Request r){return rweb::redirect("/Getting-Started", rweb::HTTP_307);});
  rweb::addRoute("/home", [](const rweb::Request r){return rweb::redirect("/Getting-Started", rweb::HTTP_307);});
  rweb::addRoute("/index", [](const rweb::Request r){return rweb::redirect("/Getting-Started", rweb::HTTP_307);});
  rweb::addRoute("/Getting-Started", &homePage);
  rweb::addRoute("/Utils", [](const rweb::Request r){
      rweb::HTMLTemplate temp = rweb::createTemplate("/utils.html", rweb::HTTP_200);
      nlohmann::json json = nlohmann::json::parse(rweb::getFileString("config.json"));
      json["CurrentURL"] = r.path;
      temp.renderJSON(json);
      return temp;
  });
  rweb::addRoute("/Templates", [](const rweb::Request r){
      rweb::HTMLTemplate temp = rweb::createTemplate("/templates.html", rweb::HTTP_200);
      nlohmann::json json = nlohmann::json::parse(rweb::getFileString("config.json"));
      json["CurrentURL"] = r.path;
      temp.renderJSON(json);
      return temp;
  });
  rweb::setErrorHandler(404, [](const rweb::Request r){return rweb::redirect("/index");});

  rweb::addResource("/style.css", "style.css", "text/css");
  rweb::addResource("/prism.css", "prism/prism.css", "text/css");
  rweb::addResource("/prism.js", "prism/prism.js", "text/javascript");

  std::cout << "----------SERVER CONFIG----------\n";
  std::cout << "Debug: " << (rweb::getDebugState() ? "ENABLED" : "DISABLED") << "\n";
  std::cout << "Resource path: " << rweb::getResourcePath() << "\n";
  std::cout << "Port: " << rweb::getPort() << "\n";
  std::cout << "----------SERVER CONFIG----------\n\n";

  std::cout << "Server started at: " << "127.0.0.1:" << rweb::getPort() << "\n\n";

  std::cout << "----------SERVER LOGS----------\n";

  if (!rweb::startServer(2))
  {
    rweb::closeServer();
    return -1;
  }

  return 0;
}

void atexit_handler()
{
  std::cout << "\n----------SERVER STOPPED----------\n";
}

static rweb::HTMLTemplate homePage(const rweb::Request r)
{
  rweb::HTMLTemplate temp = rweb::createTemplate("index.html", rweb::HTTP_200);

  //std::cout << rweb::getFileString("config.json");
  nlohmann::json json = nlohmann::json::parse(rweb::getFileString("config.json"));
  json["CurrentURL"] = r.path;
  temp.renderJSON(json);

  return temp;
}
