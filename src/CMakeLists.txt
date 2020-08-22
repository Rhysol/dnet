project(lib_dnet)

add_library(dnet_objs OBJECT
	connection_manager.cpp
	connection_manager.h
	epoll_event_manager.cpp
	epoll_event_manager.h
	io_event.h
	io_thread.cpp
	io_thread.h
	listener.cpp
	listener.h
	listener_thread.cpp
	listener_thread.h
	logger.h
	mpsc_queue.h
	net_config.h
	net_interface.h
	net_manager.cpp
	net_manager.h
	read_handler.cpp
	read_handler.h
	spsc_queue.h
	write_handler.cpp
	write_handler.h
)

target_include_directories(dnet_objs
	PUBLIC ../third_part
)

target_link_libraries(dnet_objs
	PRIVATE ../lib
)

target_link_libraries(dnet_objs
	PUBLIC pthread
	PRIVATE spdlog
)

add_library(dnet $<TARGET_OBJECTS:dnet_objs>)
