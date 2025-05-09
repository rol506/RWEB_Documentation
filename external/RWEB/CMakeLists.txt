cmake_minimum_required(VERSION 3.21 FATAL_ERROR)

#include(CTest)

project(RWEB)

#for clangd to work
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_library(RWEB

  include/RWEB.h
  include/Socket.h
  include/HTMLTemplate.h
  include/Utility.h

  include/nlohmann/json.hpp

  src/RWEB.cpp
  src/Socket.cpp
  src/HTMLTemplate.cpp
  src/Utility.cpp
)
target_include_directories(RWEB PUBLIC include)
