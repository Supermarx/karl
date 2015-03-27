project(Karl)

# find_package(SuperMarx REQUIRED)
include_directories(SYSTEM ${SuperMarx_INCLUDE_DIRS})

# find_package(librusql REQUIRED)
include_directories(SYSTEM ${librusql_INCLUDE_DIRS})

find_package(Boost COMPONENTS system program_options regex REQUIRED)

find_package(yaml-cpp REQUIRED)
include_directories(${yaml-cpp_INCLUDE_DIRS})

add_library(libkarl karl.cpp storage.cpp)
target_link_libraries(libkarl rusql ${Boost_LIBRARIES})

add_executable(karl main.cpp config.cpp)
target_link_libraries(karl libkarl ${yaml-cpp_LIBRARIES})
