#include <format>
#include <iostream>

using std::cout;
using std::format;

int main() {
    const char string[]{"this is a string"};

    for (auto* p = string; *p; p++) {
        cout << format("char = {}\n", *p);
    }

    for (int i{0}; string[i]; i++) {
        cout << format("{} char = {}\n", i, string[i]);
    }

    for (const auto& el : string) {
        cout << format("element = {}\n", el);
    }
}
