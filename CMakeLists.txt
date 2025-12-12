cmake_minimum_required(VERSION 3.16)
project(MyProject)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED True)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

message(${CMAKE_CXX_COMPILER_ID})

include(cmake/CPM.cmake)

file(GLOB_RECURSE SOURCES "${PROJECT_SOURCE_DIR}/src/*.cpp")

add_executable(${PROJECT_NAME} ${SOURCES})

file(GLOB_RECURSE HEADERS "${PROJECT_SOURCE_DIR}/*.h" "${PROJECT_SOURCE_DIR}/*.hpp")
target_include_directories(${PROJECT_NAME} PRIVATE ${PROJECT_SOURCE_DIR})
