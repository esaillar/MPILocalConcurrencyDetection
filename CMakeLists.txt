cmake_minimum_required(VERSION 3.16)
project(LocalConcurrencyDetection)

message("You need LLVM 9 or higher to build this project")

find_package(LLVM REQUIRED CONFIG)

if((LLVM_PACKAGE_VERSION VERSION_LESS 9))
  message(FATAL_ERROR "Uncompatible LLVM version found ${LLVM_PACKAGE_VERSION}")
endif()

if((LLVM_PACKAGE_VERSION VERSION_GREATER 9))
  # c++14
  set(CMAKE_CXX_STANDARD 14)
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++14")
else()
  # c++11
  set(CMAKE_CXX_STANDARD 11)
endif()

set(CMAKE_CXX_STANDARD_REQUIRED ON)

message(STATUS "Found LLVM ${LLVM_PACKAGE_VERSION} in ${LLVM_INSTALL_PREFIX}")
message(STATUS "Using LLVM binaries in: ${LLVM_TOOLS_BINARY_DIR}")
# FIXME: Not sure binary compiled with GCC would be always compatible with
# LLVM's opt. Maybe we should disable CMAKE_CXX_COMPILER and force using the
# clang provided with the LLVM release.
message(STATUS "Using CXX Compiler: ${CMAKE_CXX_COMPILER}")
message(STATUS "Using C Compiler: ${CMAKE_C_COMPILER}")

list(APPEND CMAKE_MODULE_PATH "${LLVM_CMAKE_DIR}")
include(AddLLVM)

find_package(MPI)

# Make the LLVM definitions globally available.
add_definitions(${LLVM_DEFINITIONS})
include_directories(${LLVM_INCLUDE_DIRS})

# Activate all warnings
add_compile_options(-Wall -Wno-int-to-pointer-cast)

add_subdirectory(src)
