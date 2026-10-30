#include "text_utils.hpp"

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

/**
 * Splits text into fixed-size chunks with optional overlap.
 *
 * Divides the input text into substrings of the specified chunk size.
 * If overlap is specified, chunks will share overlapping content, allowing
 * for smooth transitions between consecutive chunks. The final chunk may be
 * shorter if the text length is not evenly divisible by chunk_size.
 *
 * @param text The input string to split
 * @param chunk_size The size of each chunk in characters
 * @param overlap The number of overlapping characters between consecutive chunks
 * @return A vector of string chunks
 */
std::vector<std::string> TextUtils::split_text(const std::string& text, size_t chunk_size, size_t overlap) {
    std::vector<std::string> chunks;
    size_t start = 0;

    while (start < text.length()) {
        size_t end = std::min(start + chunk_size, text.length());
        chunks.push_back(text.substr(start, end - start));
        start = end - overlap;
        if (overlap == 0) start = end;
    }

    return chunks;
}
