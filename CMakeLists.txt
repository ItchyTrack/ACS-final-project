cmake_minimum_required(VERSION 3.16)
project(MyProject)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED True)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

message(${CMAKE_CXX_COMPILER_ID})

include(cmake/CPM.cmake)

# Fetch lz4 from GitHub
CPMAddPackage(
    NAME lz4
    GITHUB_REPOSITORY lz4/lz4
    GIT_TAG  v1.10.0
)

file(GLOB_RECURSE SOURCES "${PROJECT_SOURCE_DIR}/src/*.cpp")

add_executable(${PROJECT_NAME} ${SOURCES})

file(GLOB_RECURSE HEADERS "${PROJECT_SOURCE_DIR}/*.h" "${PROJECT_SOURCE_DIR}/*.hpp")
target_include_directories(${PROJECT_NAME} PRIVATE ${PROJECT_SOURCE_DIR} ${lz4_SOURCE_DIR}/lib)
target_link_libraries(${PROJECT_NAME} PRIVATE lz4)
add_subdirectory(${lz4_SOURCE_DIR}/build/cmake lz4-build)
