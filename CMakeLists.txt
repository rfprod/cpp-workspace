cmake_minimum_required(VERSION 3.20)

project(cpp-workspace VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

add_executable(cpp-workspace-app
    src/main.cpp
    src/ollama_client.cpp
    src/progress_spinner.cpp
)

find_package(fmt REQUIRED)
find_package(CURL REQUIRED)
find_package(nlohmann_json REQUIRED)
target_link_libraries(cpp-workspace-app
  PRIVATE
  fmt::fmt
  CURL::libcurl
  nlohmann_json::nlohmann_json
)

# enable_testing()
