project(test_lib)

add_executable(server
	main.cpp
)

target_include_directories(server
	PRIVATE ../src
)

target_include_directories(server
	PRIVATE ../third_part
)

target_link_libraries(server
	PRIVATE dnet
	PRIVATE pthread
)
