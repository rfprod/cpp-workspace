find_package(GTest CONFIG REQUIRED)

message(STATUS "Current source dir: ${CMAKE_CURRENT_SOURCE_DIR}")
file(GLOB_RECURSE TEST_SOURCES CONFIGURE_DEPENDS
     ${CMAKE_CURRENT_SOURCE_DIR}/interface/*.cpp
     ${CMAKE_CURRENT_SOURCE_DIR}/interface/*.hpp
     ${CMAKE_CURRENT_SOURCE_DIR}/ollama/*.cpp
     ${CMAKE_CURRENT_SOURCE_DIR}/ollama/*.hpp
     ${CMAKE_CURRENT_SOURCE_DIR}/storage/*.cpp
     ${CMAKE_CURRENT_SOURCE_DIR}/storage/*.hpp
     ${CMAKE_CURRENT_SOURCE_DIR}/utils/*.cpp
     ${CMAKE_CURRENT_SOURCE_DIR}/utils/*.hpp)
if(NOT TEST_SOURCES)
  message(FATAL_ERROR "No test sources found. Check the paths in TEST_SOURCES.")
endif()

add_executable(cpp-workspace-tests
  ${TEST_SOURCES}
)

target_link_libraries(cpp-workspace-tests
  PRIVATE
  # GTest::gtest_main - should be linked before `cpp-workspace-core` if the core library links `ncurses++` or similar C++ bindings that add `main()`.
  cpp-workspace-core
  GTest::gtest_main
)

include(GoogleTest)
gtest_discover_tests(cpp-workspace-tests DISCOVERY_MODE PRE_TEST)

