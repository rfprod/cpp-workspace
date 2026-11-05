find_package(GTest CONFIG REQUIRED)

add_executable(cpp-workspace-tests
  args_utils_test.cpp
)

target_link_libraries(cpp-workspace-tests
  PRIVATE
  # GTest::gtest_main - should be linked before `cpp-workspace-core` if the core library links `ncurses++` or similar C++ bindings that add `main()`.
  cpp-workspace-core
  GTest::gtest_main
)

include(GoogleTest)
gtest_discover_tests(cpp-workspace-tests DISCOVERY_MODE PRE_TEST)

