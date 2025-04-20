
project(RWEB)

add_executable(templateTest
  test.cpp
)

target_link_libraries(templateTest RWEB)

add_test(NAME templates COMMAND templateTest)
