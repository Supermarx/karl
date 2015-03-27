project(Karl)

# find_package(SuperMarx REQUIRED)
include_directories(SYSTEM ${SuperMarx_INCLUDE_DIRS})

# find_package(librusql REQUIRED)
include_directories(SYSTEM ${librusql_INCLUDE_DIRS})

find_package(Boost COMPONENTS system program_options regex REQUIRED)

add_library(libkarl karl.cpp storage.cpp)
target_link_libraries(libkarl rusql ${Boost_LIBRARIES})

add_executable(karl main.cpp)
target_link_libraries(karl libkarl)
