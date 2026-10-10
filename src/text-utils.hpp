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
    /**
     * @brief Constructs a TextUtils object.
     *
     * Default constructor. Since all methods are static, instantiation
     * is optional but permitted.
     */
    TextUtils();

    /**
     * @brief Destructs the TextUtils object.
     *
     * Default destructor. Performs no special cleanup.
     */
    ~TextUtils();

    /**
     * @brief Wraps text to fit within a specified width.
     *
     * Splits the input text into multiple lines, breaking at word boundaries
     * where possible to avoid exceeding the maximum width. Respects natural
     * spaces and removes leading spaces from wrapped lines.
     *
     * @param text The input text to wrap.
     * @param max_width The maximum width (in characters) for each line.
     * @return A vector of strings, each representing a wrapped line.
     *         The last line may be shorter than max_width.
     *
     * @note If a single word exceeds max_width, it will be placed on its own
     *       line without being broken.
     */
    static std::vector<std::string> wrap_text(const std::string& text, int max_width);
};
