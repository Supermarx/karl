cmake_minimum_required(VERSION 2.6)

add_subdirectory("contrib/librusql")
include_directories("contrib/librusql")

# find_package(SuperMarx REQUIRED)
include_directories(SYSTEM ${SuperMarx_INCLUDE_DIRS})

add_subdirectory("src")

