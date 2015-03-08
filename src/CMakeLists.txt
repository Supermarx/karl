project(Karl)

# find_package(SuperMarx REQUIRED)
include_directories(SYSTEM ${SuperMarx_INCLUDE_DIRS})

# find_package(librusql REQUIRED)
include_directories(SYSTEM ${librusql_INCLUDE_DIRS})

add_library(libkarl karl.cpp storage.cpp)
target_link_libraries(libkarl rusql_embedded)

add_executable(karl main.cpp)
target_link_libraries(karl libkarl)
