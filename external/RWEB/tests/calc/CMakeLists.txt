
project(RWEB)

add_executable(calcTest
  test_calc.cpp
)

target_link_libraries(calcTest RWEB)

add_test(NAME calculator COMMAND calcTest)
