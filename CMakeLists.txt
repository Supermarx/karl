cmake_minimum_required(VERSION 2.6)

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_CURRENT_LIST_DIR}/contrib/wubwubcmake/modules")

add_subdirectory("contrib/librusql")
set(librusql_INCLUDE_DIRS "${CMAKE_CURRENT_LIST_DIR}/contrib/librusql" ${rusql_INCLUDE_DIRS})

include("contrib/wubwubcmake/enable_cpp11.cmake")
include("contrib/wubwubcmake/warning_settings.cmake")
# add_sane_warning_flags()

add_subdirectory("src/karl")

list(APPEND Karl_INCLUDE_DIRS "${CMAKE_CURRENT_LIST_DIR}/src/")
set(Karl_INCLUDE_DIRS ${Karl_INCLUDE_DIRS} PARENT_SCOPE)
