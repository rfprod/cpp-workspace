#include <gtest/gtest.h>

#include <csignal>
#include <cstdlib>
#include <memory>
#include <vector>

#include "../tests/interface/user_interface_mock.hpp"
#include "interface/user_interface.hpp"
#include "utils/system_utils.hpp"

namespace {

// Builds the wait status for a normal exit with the given code.
constexpr int exited_with(int code) {
    return code << 8;
}

// Builds the wait status for a process stopped by a signal (WIFSTOPPED).
constexpr int stopped_by(int sig) {
    return 0x7f | (sig << 8);
}

} // namespace

class SystemUtils_catch_error : public ::testing::Test {
  protected:
    // cppcheck-suppress unusedFunction
    void SetUp() override {
        fake_ui::calls = {};
    }
};

// ---------- Zero exit codes ----------

TEST_F(SystemUtils_catch_error, zero_exit_is_silent_interactive) {
    fake_ui::calls.running = true;
    SystemUtils su(std::make_shared<UserInterface>());

    su.catch_error(exited_with(0));

    EXPECT_TRUE(fake_ui::calls.errors.empty());
    EXPECT_TRUE(fake_ui::calls.end_codes.empty());
    EXPECT_TRUE(fake_ui::calls.noninteractive.empty());
}

TEST_F(SystemUtils_catch_error, zero_exit_is_silent_non_interactive) {
    fake_ui::calls.running = false;
    SystemUtils su(std::make_shared<UserInterface>());

    su.catch_error(exited_with(0));

    EXPECT_TRUE(fake_ui::calls.errors.empty());
    EXPECT_TRUE(fake_ui::calls.end_codes.empty());
    EXPECT_TRUE(fake_ui::calls.noninteractive.empty());
}

// ---------- Non-zero exit codes ----------

TEST_F(SystemUtils_catch_error, non_zero_exit_interactive_reports_and_ends_session) {
    fake_ui::calls.running = true;
    SystemUtils su(std::make_shared<UserInterface>());

    su.catch_error(exited_with(3));

    ASSERT_EQ(fake_ui::calls.errors.size(), 1u);
    EXPECT_EQ(fake_ui::calls.errors[0], "Command failed with exit code 3.\n");
    EXPECT_EQ(fake_ui::calls.end_codes, std::vector<int>{EXIT_FAILURE});
    EXPECT_TRUE(fake_ui::calls.noninteractive.empty());
}

TEST_F(SystemUtils_catch_error, non_zero_exit_non_interactive_reports_and_ends_session) {
    fake_ui::calls.running = false;
    SystemUtils su(std::make_shared<UserInterface>());

    su.catch_error(exited_with(3));

    ASSERT_EQ(fake_ui::calls.noninteractive.size(), 1u);
    EXPECT_EQ(fake_ui::calls.noninteractive[0], "Command failed with exit code 3.\n");
    EXPECT_EQ(fake_ui::calls.end_codes, std::vector<int>{EXIT_FAILURE});
    EXPECT_TRUE(fake_ui::calls.errors.empty());
}

TEST_F(SystemUtils_catch_error, exit_code_255_is_reported_exactly) {
    fake_ui::calls.running = true;
    SystemUtils su(std::make_shared<UserInterface>());

    su.catch_error(exited_with(255));

    ASSERT_EQ(fake_ui::calls.errors.size(), 1u);
    EXPECT_EQ(fake_ui::calls.errors[0], "Command failed with exit code 255.\n");
    EXPECT_EQ(fake_ui::calls.end_codes, std::vector<int>{EXIT_FAILURE});
}

// ---------- Abnormal termination ----------

TEST_F(SystemUtils_catch_error, killed_by_signal_interactive_reports_abnormal_termination) {
    fake_ui::calls.running = true;
    SystemUtils su(std::make_shared<UserInterface>());

    su.catch_error(SIGKILL); // Low 7 bits hold the signal, so WIFEXITED is false.

    ASSERT_EQ(fake_ui::calls.errors.size(), 1u);
    EXPECT_EQ(fake_ui::calls.errors[0], "Command terminated abnormally.\n");
    EXPECT_EQ(fake_ui::calls.end_codes, std::vector<int>{EXIT_FAILURE});
}

TEST_F(SystemUtils_catch_error, killed_by_signal_non_interactive_reports_and_ends_session) {
    fake_ui::calls.running = false;
    SystemUtils su(std::make_shared<UserInterface>());

    su.catch_error(SIGKILL);

    ASSERT_EQ(fake_ui::calls.noninteractive.size(), 1u);
    EXPECT_EQ(fake_ui::calls.noninteractive[0], "Command terminated abnormally.\n");
    EXPECT_EQ(fake_ui::calls.end_codes, std::vector<int>{EXIT_FAILURE});
}

TEST_F(SystemUtils_catch_error, stopped_process_is_treated_as_abnormal) {
    fake_ui::calls.running = true;
    SystemUtils su(std::make_shared<UserInterface>());

    su.catch_error(stopped_by(SIGSTOP));

    ASSERT_EQ(fake_ui::calls.errors.size(), 1u);
    EXPECT_EQ(fake_ui::calls.errors[0], "Command terminated abnormally.\n");
    EXPECT_EQ(fake_ui::calls.end_codes, std::vector<int>{EXIT_FAILURE});
}

// ---------- system() failure (-1) ----------

TEST_F(SystemUtils_catch_error, system_failure_interactive_reports_only_once) {
    fake_ui::calls.running = true;
    SystemUtils su(std::make_shared<UserInterface>());

    su.catch_error(-1);

    ASSERT_EQ(fake_ui::calls.errors.size(), 1u);
    EXPECT_EQ(fake_ui::calls.errors[0], "system() failed.\n");
    EXPECT_EQ(fake_ui::calls.end_codes, std::vector<int>{EXIT_FAILURE});
}

TEST_F(SystemUtils_catch_error, system_failure_non_interactive_reports_only_once) {
    fake_ui::calls.running = false;
    SystemUtils su(std::make_shared<UserInterface>());

    su.catch_error(-1);

    ASSERT_EQ(fake_ui::calls.noninteractive.size(), 1u);
    EXPECT_EQ(fake_ui::calls.noninteractive[0], "system() failed.\n");
    EXPECT_EQ(fake_ui::calls.end_codes, std::vector<int>{EXIT_FAILURE});
}
