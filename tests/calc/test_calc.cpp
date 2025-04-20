#include <RWEB.h>

#include <iostream>

int main()
{
  std::string expr = "((1 +2)) * (2 + 1) * 3 ** 9";
  auto res = rweb::calculate(expr);
  if (static_cast<int>(res) == 177147)
  {
    return 0;
  }

  std::cout << "[RESULT] " << expr << " = " << res << "\n";

  return -1;
}
