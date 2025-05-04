#pragma once

#include <string>
#include <vector>

namespace rweb
{
  //returns string found in file. "" in case of an error.
  //'filePath' is the target file path from resource folder.
  std::string getFileString(const std::string& filePath);
  //remove leading and trailing spaces in 'str'.
  std::string trim(const std::string& str);
  //returns string with replaced 'from' with 'to'.
  std::string replace(const std::string& s, const std::string& from, const std::string& to) noexcept;
  //splits 's' by chars from 'seperator'.
  //'maxsplit' is the maximum count of splits. -1 by default.
  std::vector<std::string> split(const std::string& s, const std::string& seperator, int maxsplit = -1);
  //does math. Returns 0 on error.
  double calculate(const std::string& expression);
  //colorizes output. Usage: stream << colorize(color) << ... << colorize(NC) << "\n"; /*to clear color*/.
  const char *colorize(int font);

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
#elif _WIN32

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
}
