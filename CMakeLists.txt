cmake_minimum_required(VERSION 3.10)

project(LeanTeX VERSION 0.01)

set(CMAKE_CXX_STANDARD 20)

# Collect all .cpp sources under src/ so new files are picked up automatically
file(GLOB_RECURSE LEANTEX_SOURCES
	${CMAKE_SOURCE_DIR}/src/*.cpp
)

add_executable(leantex ${LEANTEX_SOURCES})

# Ensure the src directory is available for headers like "loader.hpp"
target_include_directories(leantex PRIVATE ${CMAKE_SOURCE_DIR}/src)