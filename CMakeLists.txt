cmake_minimum_required(VERSION 3.20)

project(cpp-workspace VERSION 0.2.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

add_executable(cpp-workspace-app
  src/main.cpp
  src/interface/progress_spinner.cpp
  src/interface/user-interface.cpp
  src/ollama/ollama_client.cpp
  src/utils/text-utils.cpp
)

target_include_directories(cpp-workspace-app
  PRIVATE
  ${CMAKE_SOURCE_DIR}/src
  ${CMAKE_SOURCE_DIR}/src/interface
  ${CMAKE_SOURCE_DIR}/src/ollama
  ${CMAKE_SOURCE_DIR}/src/utils
)

find_package(fmt REQUIRED)
find_package(CURL REQUIRED)
find_package(nlohmann_json REQUIRED)
find_package(Curses REQUIRED)
target_link_libraries(cpp-workspace-app
  PRIVATE
  fmt::fmt
  CURL::libcurl
  nlohmann_json::nlohmann_json
  Curses::Curses
)
