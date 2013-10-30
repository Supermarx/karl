cmake_minimum_required(VERSION 2.6)

add_subdirectory("contrib/librusql")
include_directories("contrib/librusql")

add_subdirectory("src")

