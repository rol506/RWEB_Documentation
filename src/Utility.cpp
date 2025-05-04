#include "../include/Utility.h"

#include <iostream>
#include <cmath>

namespace rweb
{

  std::string trim(const std::string& str)
  {
    static const std::string ws = " \t\n";
    auto first = str.find_first_not_of(ws);
    auto last = str.find_last_not_of(ws);
    return first == std::string::npos ? "" : str.substr(first, last-first+1);
  }

  std::vector<std::string> split(const std::string& s, const std::string& seperator, int maxsplit)
  {
    int spt = maxsplit+1;
    std::vector<std::string> output;
    std::string::size_type prev_pos = 0, pos = 0;

    if (spt <= 0)
    {
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
    } else {
      while ((pos = s.find_first_of(seperator, pos)) != std::string::npos && spt > 1)
      {
        std::string substring(trim(s.substr(prev_pos, pos-prev_pos)));
        if (substring.size() > 0)
        {
          output.push_back(substring);
        }
        prev_pos = ++pos;
        spt--;
      }
      output.push_back(trim(s.substr(prev_pos, s.size()-prev_pos)));
      return output;
    }
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
    if (trim(expression) == "")
    {
      std::cout << colorize(YELLOW) << "[MATH] WARNING! Calculating an empty expression! Result will be 0" << colorize(NC) << "\n";
      return 0;
    }
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
        std::cerr << "[CALC] ERROR FOUND UNCLOSED BRACKET!\n";
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
}
