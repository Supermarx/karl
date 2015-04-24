cmake_minimum_required(VERSION 2.6)

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_CURRENT_LIST_DIR}/contrib/wubwubcmake/modules")

include("contrib/wubwubcmake/enable_cpp11.cmake")
include("contrib/wubwubcmake/warning_settings.cmake")
include("contrib/wubwubcmake/xxd_process.cmake")
add_sane_warning_flags()

list(APPEND Karl_INCLUDE_DIRS "${CMAKE_CURRENT_LIST_DIR}/src/")

add_subdirectory("src/karl")

set(Karl_INCLUDE_DIRS ${Karl_INCLUDE_DIRS} PARENT_SCOPE)
