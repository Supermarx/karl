cmake_minimum_required(VERSION 2.6)

add_subdirectory("contrib/librusql")
set(librusql_INCLUDE_DIRS "${CMAKE_CURRENT_LIST_DIR}/contrib/librusql" ${rusql_INCLUDE_DIRS})

include("contrib/wubwubcmake/enable_cpp11.cmake")
include("contrib/wubwubcmake/warning_settings.cmake")
add_sane_warning_flags()

add_subdirectory("src")
