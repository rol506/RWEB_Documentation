#include <iostream>
#include <cstdlib>

#include <RWEB.h>


int main()
{
  rweb::setResourcePath("/home/rol506/proj/C++/RWEB/tests/template/res/");
  rweb::HTMLTemplate temp = rweb::createTemplate("index.html", rweb::HTTP_200);

  nlohmann::json json = nlohmann::json::parse(rweb::getFileString("../menu.json"));

  temp.renderJSON(json);

  std::cout << "RESULT HTML: " << temp.getHTML();

  if (temp.getHTML() != rweb::getFileString("result.html"))
  {
    return -1;
  }

  return 0;
}
