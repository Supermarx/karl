project(karl)

# find_package(SuperMarx REQUIRED)
list(APPEND Karl_INCLUDE_DIRS ${SuperMarx_INCLUDE_DIRS})

# find_package(librusql REQUIRED)
list(APPEND Karl_INCLUDE_DIRS ${librusql_INCLUDE_DIRS})

find_package(Boost COMPONENTS system program_options regex REQUIRED)

find_package(yaml-cpp REQUIRED)
list(APPEND Karl_INCLUDE_DIRS ${yaml-cpp_INCLUDE_DIRS})

find_package(fastcgipp REQUIRED)
list(APPEND Karl_INCLUDE_DIRS ${fastcgipp_INCLUDE_DIRS})

add_library(libkarl karl.cpp storage.cpp config.cpp)
target_link_libraries(libkarl rusql ${Boost_LIBRARIES} ${yaml-cpp_LIBRARIES})

add_executable(karl main.cpp)
target_link_libraries(karl libkarl ${fastcgipp_LIBRARIES})

include_directories(SYSTEM ${Karl_INCLUDE_DIRS})
set(Karl_INCLUDE_DIRS ${Karl_INCLUDE_DIRS} PARENT_SCOPE)
