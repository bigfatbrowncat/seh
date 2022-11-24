cmake_minimum_required(VERSION 3.10)
set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

project(SEH)
add_executable(seh_test seh_test.cpp)
target_link_libraries(seh_test pthread)

#add_executable(seh_lite_test seh_lite_test.c)
