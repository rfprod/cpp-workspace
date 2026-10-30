#pragma once

#include <string>
#include <vector>

/**
 * @class TextUtils
 * @brief Utility class for text manipulation operations.
 *
 * Provides static methods for common text processing tasks, including
 * line wrapping based on a specified maximum width.
 */
class TextUtils {
  public:
    static std::vector<std::string> wrap_text(const std::string& text, int max_width);

    static std::vector<std::string> split_text(const std::string& text, size_t chunk_size, size_t overlap = 0);
};
