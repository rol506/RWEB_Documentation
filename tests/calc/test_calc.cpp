#include <RWEB.h>

#include <iostream>

int main()
{
  rweb::init(false);
  std::string expr = "((1 +2)) * (2 + 1) * 3 ** 9";
  auto res = rweb::calculate(expr);
  if (static_cast<int>(res) != 177147)
  {
    return -1;
  }
  std::cout << "[RESULT] " << "(" << expr << ") = " << rweb::colorize(rweb::GREEN) <<  res << rweb::colorize(rweb::NC) << "\n";

  expr = "";
  res = rweb::calculate(expr);
  if (static_cast<int>(res) != 0)
  {
    return -1;
  }
  std::cout << "[RESULT] " << "(" << expr << ") = " << rweb::colorize(rweb::GREEN) << res << rweb::colorize(rweb::NC) << "\n";

  return 0;
}
