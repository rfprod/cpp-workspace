#pragma once

#include <string>
#include <vector>

namespace fake_ui {
struct Calls {
    bool running = false;
    std::vector<std::string> errors;
    std::vector<int> end_codes;
    std::vector<std::string> noninteractive;
};
extern Calls calls;
} // namespace fake_ui
