project(karl)

# find_package(SuperMarx REQUIRED)
list(APPEND Karl_INCLUDE_DIRS ${SuperMarx_INCLUDE_DIRS})

# find_package(librusql REQUIRED)
list(APPEND Karl_INCLUDE_DIRS ${librusql_INCLUDE_DIRS})

find_package(Boost COMPONENTS system program_options regex chrono date_time thread filesystem REQUIRED)

find_package(yaml-cpp REQUIRED)
list(APPEND Karl_INCLUDE_DIRS ${yaml-cpp_INCLUDE_DIRS})

find_package(fastcgipp REQUIRED)
list(APPEND Karl_INCLUDE_DIRS ${fastcgipp_INCLUDE_DIRS})

find_package(pqxx REQUIRED)
list(APPEND Karl_INCLUDE_DIRS ${pqxx_INCLUDE_DIRS})

find_package(ImageMagick COMPONENTS Magick++ REQUIRED)
list(APPEND Karl_INCLUDE_DIRS ${ImageMagick_INCLUDE_DIRS})

find_package(scrypt REQUIRED)
list(APPEND Karl_INCLUDE_DIRS ${scrypt_INCLUDE_DIRS})

file(GLOB_RECURSE QUERY_FILES RELATIVE ${CMAKE_CURRENT_LIST_DIR} sql/*)              
xxd_process(Karl_SQL "${CMAKE_CURRENT_BINARY_DIR}/sql.cc" "${QUERY_FILES}" "supermarx")
include_directories(${CMAKE_CURRENT_BINARY_DIR}) # Include directory where Karl_SQL is generated

add_library(karlcore karl.cpp storage.cpp config.cpp util/log.cpp image_citations.cpp ${Karl_SQL})
target_link_libraries(karlcore
	${pqxx_LIBRARIES}
	${yaml-cpp_LIBRARIES}
	${Boost_LIBRARIES}
	${ImageMagick_LIBRARIES}
	${scrypt_LIBRARIES}
	)

add_library(karlapi api/api_server.cpp api/request.cpp api/uri.cpp api/response_handler.cpp)
target_link_libraries(karlapi
	karlcore
	supermarx-serialization-xml
	supermarx-serialization-msgpack
	supermarx-serialization-json
	${fastcgipp_LIBRARIES})

add_executable(karl main.cpp)
target_link_libraries(karl karlcore karlapi)

include_directories(SYSTEM ${Karl_INCLUDE_DIRS})
set(Karl_INCLUDE_DIRS ${Karl_INCLUDE_DIRS} PARENT_SCOPE)
