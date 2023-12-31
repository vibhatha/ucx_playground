cmake_minimum_required(VERSION 3.10)
project(MyUCXPlayGround)

# Enable C++11
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

set(UCX_DIR "$ENV{UCX_HOME}/build")

# Find the UCX include directory and libraries
find_path(UCX_INCLUDE_DIR NAMES ucp/api/ucp.h HINTS ${UCX_DIR}/include)
find_library(UCX_LIBRARY_UCP NAMES ucp HINTS ${UCX_DIR}/lib)
find_library(UCX_LIBRARY_UCS NAMES ucs HINTS ${UCX_DIR}/lib)

# Include directories
include_directories(${UCX_INCLUDE_DIR})

# Helper function to create a new target
function(create_target target_name source_files)
    add_executable(${target_name} ${source_files})
    target_link_libraries(${target_name} ${UCX_LIBRARY_UCP} ${UCX_LIBRARY_UCS})
endfunction()

create_target(main "src/main.cpp")
create_target(hello_ucx "src/hello_ucx.cpp")