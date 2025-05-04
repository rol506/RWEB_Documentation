cmake_minimum_required(VERSION 3.21 FATAL_ERROR)

project(RWEB)

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
