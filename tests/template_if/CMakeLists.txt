
project(RWEB)

add_executable(templateTestIf
  test.cpp
)

target_link_libraries(templateTestIf RWEB)

add_test(NAME templatesIf COMMAND templateTestIf)
