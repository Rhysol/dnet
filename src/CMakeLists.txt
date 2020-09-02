cmake_minimum_required(VERSION 3.15)

project(lib)

add_library(dnet
	connection.h
	connection.cpp
	io_event.h
	io_thread.cpp
	io_thread.h
	listener_thread.cpp
	listener_thread.h
	logger.h
	mpsc_queue.h
	net_config.h
	net_interface.h
	net_manager.cpp
	net_manager.h
	spsc_queue.h
)

target_include_directories(dnet
	PUBLIC ../third_part
)

target_link_directories(dnet
	PUBLIC ../lib
)

target_link_libraries(dnet
	PUBLIC pthread
	PUBLIC spdlog
)
