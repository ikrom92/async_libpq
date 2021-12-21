cmake_minimum_required(VERSION 3.10)
set(CMAKE_DEBUG_POSTFIX d)

project(async_libpq)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++17 -pthread")
set(CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR})

include_directories(/usr/local/opt/postgres/include)
include_directories(/usr/opt/)
link_directories(/usr/local/opt/postgres/lib)
file(GLOB SOURCES "src/*.cpp" "src/**/*.cpp" "test/*.cpp")
add_executable(async_libpq ${SOURCES})
target_link_libraries(async_libpq -lpq)
set_target_properties(async_libpq PROPERTIES DEBUG_POSTFIX ${CMAKE_DEBUG_POSTFIX})