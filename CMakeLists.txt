cmake_minimum_required (VERSION 3.16)

project (AgFileSystemScanner LANGUAGES C CXX)
set (CMAKE_CXX_STANDARD 20)
set (CMAKE_CXX_STANDARD_REQUIRED ON)

include_directories (${CMAKE_SOURCE_DIR}/include)
add_subdirectory (${CMAKE_SOURCE_DIR}/src)
