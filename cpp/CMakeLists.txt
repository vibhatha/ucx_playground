cmake_minimum_required(VERSION 3.10)
project(MyUCXPlayGround)

# Enable C++11
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

# Set the module path to include the FindUCX.cmake file
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

# Find UCX using the FindUCX module
find_package(UCX REQUIRED)

# Include directories
include_directories(${UCX_INCLUDE_DIRS})

# Add your header files into a variable
set(HEADER_FILES
        src/common_utils.h
        src/memory_utils.h
        src/ucx_utils.h
        src/data_util.h
        src/ucx_config.h
        src/ucp_client.h
        src/ucp_server.h
)

set(SOURCE_FILES
        src/data_util.cpp
        src/memory_utils.cpp
        src/ucx_config.cpp
        src/ucx_utils.cpp
        src/ucp_client.cpp
        src/ucp_server.cpp
        # Add other source files here
)

add_library(my_ucx_lib SHARED ${SOURCE_FILES})

# Make sure compiler can find your header files
target_include_directories(my_ucx_lib PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)

target_link_libraries(my_ucx_lib ${UCX_LIBRARIES})

# Helper function to create a new target
function(create_target target_name source_files)
    add_executable(${target_name} ${source_files})
    target_link_libraries(${target_name} ${UCX_LIBRARIES} my_ucx_lib)
endfunction()

create_target(main "src/main.cpp")
create_target(hello_ucx "src/hello_ucx.cpp")
# create_target(hello_ucp "src/ucp_hello_world.c")
create_target(hello_epoll "src/hello_epoll.c")
create_target(ucp_hello_v2 "src/ucp_hello_world_v2.cpp")
