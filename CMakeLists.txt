cmake_minimum_required(VERSION 3.21 FATAL_ERROR)

project(RWEB)

#for clangd to work
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_library(RWEB

  include/RWEB.h

  include/nlohmann/json.hpp

  src/RWEB.cpp
)
target_include_directories(RWEB PUBLIC include)

add_executable(test
  test.cpp
)

target_link_libraries(test RWEB)

add_custom_command(TARGET test POST_BUILD COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_SOURCE_DIR}/res $<TARGET_FILE_DIR:${PROJECT_NAME}>/res)
