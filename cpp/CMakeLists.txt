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

# Helper function to create a new target
function(create_target target_name source_files)
    add_executable(${target_name} ${source_files})
    target_link_libraries(${target_name} ${UCX_LIBRARIES})
endfunction()

create_target(main "src/main.cpp")
create_target(hello_ucx "src/hello_ucx.cpp")
create_target(hello_ucp "src/ucp_hello_world.c")
create_target(hello_epoll "src/hello_epoll.c")