cmake_minimum_required(VERSION 3.23)
project(EnergyPass C CXX)

set(LLVM_DIR "/usr/lib/llvm-17/lib/cmake/llvm" CACHE PATH "LLVM cmake dir")

find_package(LLVM REQUIRED CONFIG)
message(STATUS "Found LLVM ${LLVM_VERSION} at ${LLVM_DIR}")

list(APPEND CMAKE_MODULE_PATH "${LLVM_CMAKE_DIR}")
include(AddLLVM)

add_definitions(${LLVM_DEFINITIONS})
include_directories(${LLVM_INCLUDE_DIRS})

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_subdirectory(lib)
