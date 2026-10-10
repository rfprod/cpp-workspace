#include "text-utils.hpp"

TextUtils::TextUtils() {
}

TextUtils::~TextUtils() {
}

/**
 * @brief Implementation of wrap_text method.
 *
 * Algorithm:
 * 1. Iteratively extract lines of up to max_width characters
 * 2. Prefer breaking at word boundaries (spaces)
 * 3. If no space exists within max_width, break at max_width
 * 4. Remove leading whitespace from each new line
 * 5. Continue until remaining text fits within max_width
 */
std::vector<std::string> TextUtils::wrap_text(const std::string& text, int max_width) {
    std::vector<std::string> result;
    std::string remaining = text;

    while (remaining.length() > max_width) {
        // Find the last space within max_width.
        int wrap_position = remaining.rfind(' ', max_width);

        if (wrap_position == std::string::npos) {
            wrap_position = max_width;
        }

        // Add the wrapped line without trailing space.
        std::string line = remaining.substr(0, wrap_position);
        result.push_back(line);

        // Skip the space and continue with remainder.
        remaining = remaining.substr(wrap_position);

        // Trim leading spaces from next line.
        size_t first_non_space = remaining.find_first_not_of(' ');
        if (first_non_space != std::string::npos) {
            remaining = remaining.substr(first_non_space);
        } else {
            break;
        }
    }

    if (!remaining.empty()) {
        result.push_back(remaining);
    }

    return result;
}
