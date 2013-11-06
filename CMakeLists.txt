cmake_minimum_required(VERSION 2.6)

add_subdirectory("contrib/librusql")
include_directories("contrib/librusql")

# find_package(SuperMarx REQUIRED)
include_directories(SYSTEM ${SuperMarx_INCLUDE_DIRS})

include("contrib/wubwubcmake/enable_cpp11.cmake")
include("contrib/wubwubcmake/warning_settings.cmake")
add_sane_warning_flags()

add_subdirectory("src")

