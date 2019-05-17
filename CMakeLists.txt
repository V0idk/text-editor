# cmake_minimum_required(VERSION <specify CMake version here>)
project(editor)
set(CMAKE_CXX_COMPILER "/usr/bin/g++")
set(CMAKE_CXX_FLAGS "-fpermissive -g -Wall -W -pedantic -std=c++17")

add_executable(editor main.cc)