cmake_minimum_required(VERSION 3.15)
project(test_lib)

add_executable(server
	server.cpp
)
target_include_directories(server
	PRIVATE ../src
	PRIVATE ../third_part
)
target_link_libraries(server
	PRIVATE dnet
	PRIVATE pthread
)

add_executable(client
	client.cpp
)
target_include_directories(client
	PRIVATE ../src
	PRIVATE ../third_part
)
target_link_libraries(client
	PRIVATE dnet
	PRIVATE pthread
)

add_executable(short_link_client
	short_link_client.cpp
)
target_include_directories(short_link_client
	PRIVATE ../src
	PRIVATE ../third_part
)
target_link_libraries(short_link_client
	PRIVATE dnet
	PRIVATE pthread
)

add_executable(spsc_queue_test
	test_spsc_queue.cpp
)
target_include_directories(spsc_queue_test
	PRIVATE ../src
)
target_link_libraries(spsc_queue_test
	PRIVATE dnet
	PRIVATE pthread
)

add_executable(mpsc_queue_test
	test_mpsc_queue.cpp
)
target_include_directories(mpsc_queue_test
	PRIVATE ../src
)
target_link_libraries(mpsc_queue_test
	PRIVATE dnet
	PRIVATE pthread
)
