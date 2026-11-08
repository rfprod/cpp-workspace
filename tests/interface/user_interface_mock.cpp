#include "interface/user_interface.hpp"

#include <cstdio>
#include <string>
#include <vector>

namespace fake_ui {

struct Calls {
    bool running = false;
    std::vector<std::string> errors;
    std::vector<int> end_codes;
    std::vector<std::string> noninteractive;
};

inline Calls calls; // Test code resets and inspects this.

} // namespace fake_ui

UserInterface::UserInterface() : input_cursor_x(0), layout({}), use_dark_theme(true) {
}

UserInterface::UserInterface(bool dark_theme) : input_cursor_x(0), layout({}), use_dark_theme(dark_theme) {
}

UserInterface::~UserInterface() = default;

bool UserInterface::ui_running(const std::shared_ptr<UserInterface>&) {
    return fake_ui::calls.running;
}

void UserInterface::print_error(const std::string& text) {
    fake_ui::calls.errors.push_back(text);
}

void UserInterface::end_session(int code) {
    fake_ui::calls.end_codes.push_back(code);
}

void UserInterface::print_error_noninteractive(const std::string& text) {
    fake_ui::calls.noninteractive.push_back(text);
    std::fputs(text.c_str(), stderr);
}
