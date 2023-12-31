# FindUCX.cmake

# Set a variable for the UCX directory
set(UCX_DIR "$ENV{UCX_HOME}/build" CACHE PATH "UCX installation directory")

# Find the UCX include directory and libraries
find_path(UCX_INCLUDE_DIR NAMES ucp/api/ucp.h HINTS ${UCX_DIR}/include)
find_library(UCX_LIBRARY_UCP NAMES ucp HINTS ${UCX_DIR}/lib)
find_library(UCX_LIBRARY_UCS NAMES ucs HINTS ${UCX_DIR}/lib)

# Handle the QUIETLY and REQUIRED arguments and set UCX_FOUND to TRUE if all listed variables are TRUE
include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(UCX DEFAULT_MSG UCX_INCLUDE_DIR UCX_LIBRARY_UCP UCX_LIBRARY_UCS)

if(UCX_FOUND)
    set(UCX_INCLUDE_DIRS ${UCX_INCLUDE_DIR})
    set(UCX_LIBRARIES ${UCX_LIBRARY_UCP} ${UCX_LIBRARY_UCS})
endif()

mark_as_advanced(UCX_DIR UCX_INCLUDE_DIR UCX_LIBRARY_UCP UCX_LIBRARY_UCS)
