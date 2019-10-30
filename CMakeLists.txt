cmake_minimum_required (VERSION 3.1)
project (net_lib)

set (NET_LIB_VERSION_MAJOR 1)
set (NET_LIB_VERSION_MINOR 0)

include_directories(${PROJECT_SOURCE_DIR}/src)
link_directories(${PROJECT_SOURCE_DIR})

set (CMAKE_CXX_STANDARD 11)

add_library(obj_net OBJECT src/listener.cpp src/connection_manager.cpp src/epoll_event_manager.cpp
src/read_handler.cpp src/write_handler.cpp src/io_thread.cpp src/listener_thread.cpp src/net_manager.cpp)
target_link_libraries(obj_net pthread profiler tcmalloc)

set_property(TARGET obj_net PROPERTY POSITION_INDEPENDENT_CODE 1)

add_library(net SHARED $<TARGET_OBJECTS:obj_net>)

add_library(net_static STATIC $<TARGET_OBJECTS:obj_net>)
set_target_properties(net_static PROPERTIES OUTPUT_NAME net)

add_executable(server test/main.cpp)
target_link_libraries(server net)
