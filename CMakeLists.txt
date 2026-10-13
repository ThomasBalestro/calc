cmake_minimum_required(VERSION 3.16)
project(calc CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(MSVC)
  add_compile_options(/W4 /utf-8)
else()
  add_compile_options(-Wall -Wextra -Wpedantic -Wshadow)
endif()

add_library(calc_core STATIC src/lexer.cpp src/parser.cpp src/interpreter.cpp)
target_include_directories(calc_core PUBLIC src)

add_executable(calc src/main.cpp)
target_link_libraries(calc PRIVATE calc_core)

enable_testing()
add_executable(test_calc tests/test_calc.cpp)
target_link_libraries(test_calc PRIVATE calc_core)
add_test(NAME calc COMMAND test_calc)
