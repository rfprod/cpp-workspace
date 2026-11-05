cmake_minimum_required(VERSION 3.20)

project(cpp-workspace VERSION 0.4.4 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

option(CPP_WORKSPACE_BUILD_TESTS "Build unit tests" ON)
option(ENABLE_COVERAGE "Build with gcov instrumentation" OFF)

if(ENABLE_COVERAGE)
  if(NOT CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    message(FATAL_ERROR "ENABLE_COVERAGE requires GCC")
  endif()
  add_compile_options(--coverage -O0 -g)
  add_link_options(--coverage)
endif()

message(STATUS "Current source dir: ${CMAKE_CURRENT_SOURCE_DIR}")
file(GLOB_RECURSE CORE_SOURCES CONFIGURE_DEPENDS
     ${CMAKE_CURRENT_SOURCE_DIR}/src/interface/*.cpp
     ${CMAKE_CURRENT_SOURCE_DIR}/src/interface/*.hpp
     ${CMAKE_CURRENT_SOURCE_DIR}/src/ollama/*.cpp
     ${CMAKE_CURRENT_SOURCE_DIR}/src/ollama/*.hpp
     ${CMAKE_CURRENT_SOURCE_DIR}/src/storage/*.cpp
     ${CMAKE_CURRENT_SOURCE_DIR}/src/storage/*.hpp
     ${CMAKE_CURRENT_SOURCE_DIR}/src/utils/*.cpp
     ${CMAKE_CURRENT_SOURCE_DIR}/src/utils/*.hpp)
if(NOT CORE_SOURCES)
  message(FATAL_ERROR "No core sources found. Check the paths in CORE_SOURCES.")
endif()
add_library(cpp-workspace-core
  STATIC
  ${CORE_SOURCES})
target_include_directories(cpp-workspace-core
  PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}/src)

add_executable(cpp-workspace-app src/main.cpp)
target_link_libraries(cpp-workspace-app PRIVATE cpp-workspace-core)

find_package(fmt REQUIRED)
find_package(CURL REQUIRED)
find_package(nlohmann_json REQUIRED)
find_package(Curses REQUIRED)
target_link_libraries(cpp-workspace-core
  PRIVATE
  fmt::fmt
  CURL::libcurl
  nlohmann_json::nlohmann_json
  Curses::Curses
)

if(CPP_WORKSPACE_BUILD_TESTS)
  enable_testing()
  add_subdirectory(tests)
endif()
