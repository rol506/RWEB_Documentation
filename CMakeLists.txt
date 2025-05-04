cmake_minimum_required(VERSION 3.21 FATAL_ERROR)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

project(rwebDocs)

add_executable(app
  src/main.cpp
)

add_subdirectory(external/RWEB)

target_link_libraries(app RWEB)

add_custom_command(TARGET app POST_BUILD COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_SOURCE_DIR}/res $<TARGET_FILE_DIR:app>/res)
