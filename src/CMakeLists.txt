project(Karl)

add_library(libkarl karl.cpp karl.hpp)

add_executable(karl main.cpp)
target_link_libraries(karl libkarl)
