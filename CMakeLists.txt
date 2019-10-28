cmake_minimum_required (VERSION 3.1)
project (net_lib)

set (NET_LIB_VERSION_MAJOR 1)
set (NET_LIB_VERSION_MINOR 0)

include_directories(${PROJECT_SOURCE_DIR}/src ${PROJECT_BINARY_DIR}/bin)

set (CMAKE_CXX_STANDARD 11)

add_executable(net_lib src/main.cpp src/listener.cpp src/connection_manager.cpp src/epoll_event_manager.cpp
src/io_handler.cpp src/io_thread.cpp src/listener_thread.cpp src/net_manager.cpp)
target_link_libraries(net_lib pthread profiler tcmalloc)
