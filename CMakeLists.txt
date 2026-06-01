cmake_minimum_required(VERSION 3.23)
project(EnergyPass C CXX)

if(NOT DEFINED LLVM_DIR OR LLVM_DIR STREQUAL "")
  set(LLVM_DIR "${CMAKE_SOURCE_DIR}/build/lib/cmake/llvm")
endif()

find_package(LLVM REQUIRED CONFIG)
message(STATUS "Found LLVM ${LLVM_VERSION} at ${LLVM_DIR}")

list(APPEND CMAKE_MODULE_PATH "${LLVM_CMAKE_DIR}")
include(AddLLVM)

add_definitions(${LLVM_DEFINITIONS})

# Both are required:
# - llvm-project/llvm/include  → source headers (IR, Support, etc.)
# - build/include              → generated headers (PassPlugin.h, intrinsics, etc.)
include_directories(${LLVM_INCLUDE_DIRS})
include_directories(${LLVM_INCLUDE_DIRS})
if(EXISTS "${CMAKE_SOURCE_DIR}/build/include")
  include_directories(${CMAKE_SOURCE_DIR}/build/include)
endif()
include_directories(${CMAKE_SOURCE_DIR}/include)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(MSVC)
  add_compile_options(/W3 /wd4624 /wd4267 /wd4244 /EHsc)
  add_definitions(-D_SILENCE_ALL_CXX17_DEPRECATION_WARNINGS)
endif()

add_subdirectory(lib)
