cmake_minimum_required(VERSION 3.17)

project(dnet)

set(CMAKE_CXX_STANDARD 11)

set(CMAKE_CXX_FLAGS "-g -Wall -Wextra -fPIC")
set(CMAKE_CXX_FLAGS_RELEASE "-O3")


add_subdirectory(src)
add_subdirectory(test)

