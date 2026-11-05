#include "text_utils.hpp"
#include <algorithm>
#include <cctype>
#include <stdexcept>

/**
 * Abbreviations that end with a period but do not end a sentence.
 * Entries are lowercase and include the trailing period.
 */
const std::vector<std::string> TextUtils::abbreviationExceptions = {"e.g.", "i.e.", "vs.",   "approx.", "dr.", "mr.",
                                                                    "mrs.", "ms.",  "prof.", "jr.",     "sr.", "st.",
                                                                    "fig.", "no.",  "vol.",  "pp.",     "ca."};

/**
 * Wraps text to fit within a maximum line width.
 *
 * Splits the input text into lines, breaking at the last space that fits
 * within max_width. Spaces at the start of each wrapped line and at the end
 * of each line are removed.
 *
 * @param text The input text to wrap.
 * @param max_width The maximum number of characters per line. Must be greater than 0.
 * @return A vector of wrapped lines. Each line is at most max_width characters.
 *         The last line may be shorter.
 *
 * @throws std::invalid_argument if max_width is 0 or negative.
 *
 * @note If a single word is longer than max_width, it is broken at max_width
 *       characters. Only space characters are treated as break points.
 *
 * Algorithm:
 * 1. Remove leading spaces from the text.
 * 2. While the remaining text is longer than max_width, find the last space
 *    at or before max_width.
 * 3. If no space exists, break at max_width.
 * 4. Emit the line without trailing spaces, then remove leading spaces from the rest.
 * 5. Emit any remaining text.
 */
std::vector<std::string> TextUtils::wrap_text(const std::string& text, const int max_width) {
    if (max_width <= 0) {
        throw std::invalid_argument("max_width must be greater than 0");
    }

    const size_t width = static_cast<size_t>(max_width);
    std::vector<std::string> result;

    const size_t first = text.find_first_not_of(' ');
    std::string remaining = (first == std::string::npos) ? std::string() : text.substr(first);

    while (remaining.length() > width) {
        // Find the last space within the width, or break at the width if none exists.
        size_t wrap_position = remaining.rfind(' ', width);
        if (wrap_position == std::string::npos) {
            wrap_position = width;
        }

        // Emit the line without trailing spaces. The first character is never a space
        // here, so the line is never empty.
        std::string line = remaining.substr(0, wrap_position);
        line.erase(line.find_last_not_of(' ') + 1);
        result.push_back(line);

        // Move past the break and remove leading spaces from the next line.
        remaining = remaining.substr(wrap_position);
        const size_t next_start = remaining.find_first_not_of(' ');
        remaining = (next_start == std::string::npos) ? std::string() : remaining.substr(next_start);
    }

    if (!remaining.empty()) {
        result.push_back(remaining);
    }

    return result;
}

/**
 * Checks whether a character is whitespace.
 *
 * @param c The character to test.
 * @return true if c is a space, tab, newline, or other whitespace character.
 */
bool TextUtils::is_space(const char c) {
    return std::isspace(static_cast<unsigned char>(c)) != 0;
}

/**
 * Checks whether the token ending before a whitespace character is an abbreviation.
 *
 * Extracts the token that ends at space_idx, strips leading punctuation such as
 * '(' or '"', converts it to lowercase, and compares it to abbreviationExceptions.
 *
 * @param text The full input text.
 * @param space_idx The index of the whitespace character that follows the token.
 * @return true if the token is in abbreviationExceptions.
 */
bool TextUtils::is_abbreviation(const std::string& text, const size_t space_idx) {
    size_t word_start = space_idx;
    while (word_start > 0 && !is_space(text[word_start - 1])) {
        word_start -= 1;
    }

    // Strip leading punctuation so "(e.g." still matches.
    while (word_start < space_idx && !std::isalnum(static_cast<unsigned char>(text[word_start]))) {
        word_start += 1;
    }

    std::string word = text.substr(word_start, space_idx - word_start);
    std::transform(word.begin(), word.end(), word.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

    return std::find(abbreviationExceptions.begin(), abbreviationExceptions.end(), word)
           != abbreviationExceptions.end();
}

/**
 * Checks whether a whitespace character marks the end of a sentence.
 *
 * A sentence ends at whitespace preceded by '!' or '?', or by '.' when the
 * preceding token is not an abbreviation.
 *
 * @param text The full input text.
 * @param i The index to test. Must be less than text.length().
 * @return true if index i is a sentence boundary.
 */
bool TextUtils::is_sentence_boundary(const std::string& text, const size_t i) {
    if (i == 0 || !is_space(text[i])) {
        return false;
    }

    const char prev = text[i - 1];
    if (prev == '!' || prev == '?') {
        return true;
    }
    if (prev == '.') {
        return !is_abbreviation(text, i);
    }
    return false;
}

/**
 * Finds the best position to end a chunk within a window.
 *
 * Searches backward from end for a sentence boundary first, then for any
 * whitespace. Only positions at or after start + min_len are considered,
 * so the resulting chunk is at least min_len characters long.
 *
 * @param text The full input text.
 * @param start The index where the current chunk begins.
 * @param end The maximum index for the chunk end. Must be less than text.length().
 * @param min_len The minimum chunk length in characters.
 * @return The break index, or 0 if no suitable break exists.
 */
size_t TextUtils::find_break(const std::string& text, const size_t start, const size_t end, const size_t min_len) {
    const size_t lo = start + std::max<size_t>(1, min_len); // chunk must be at least min_len long
    if (lo > end) {
        return 0;
    }

    for (size_t i = end; i >= lo; i -= 1) {
        if (is_sentence_boundary(text, i)) {
            return i;
        }
    }

    for (size_t i = end; i >= lo; i -= 1) {
        if (is_space(text[i])) {
            return i;
        }
    }

    return 0;
}

/**
 * Splits text into sentence-aware chunks with optional overlap.
 *
 * Each chunk is at most chunk_size characters. The method prefers to end a
 * chunk at a sentence boundary, then at any whitespace, and only splits inside
 * a word when the window contains no whitespace in its second half. Chunks are
 * at least chunk_size / 2 characters long, except for the final chunk.
 *
 * Consecutive chunks share approximately overlap characters. The next chunk
 * starts at the earliest sentence start inside the overlap region, or at the
 * nearest word start if there is none. The overlap is therefore at most
 * overlap characters and may be smaller.
 *
 * Leading and trailing whitespace is removed from each chunk.
 *
 * @param text The input text to split.
 * @param chunk_size The maximum size of each chunk in characters. Must be greater than 0.
 * @param overlap The approximate number of characters shared between consecutive chunks.
 *                Must be less than chunk_size.
 * @return A vector of chunks. Returns an empty vector if text contains only whitespace.
 *
 * @throws std::invalid_argument if chunk_size is 0 or overlap is greater than or equal to chunk_size.
 */
std::vector<std::string> TextUtils::split_text(const std::string& text, const size_t chunk_size, const size_t overlap) {
    if (chunk_size == 0) {
        throw std::invalid_argument("chunk_size must be greater than 0");
    }
    if (overlap >= chunk_size) {
        throw std::invalid_argument("overlap must be smaller than chunk_size");
    }

    const size_t min_len = chunk_size / 2;
    std::vector<std::string> chunks;
    const size_t text_length = text.length();
    size_t start = 0;

    while (start < text_length && is_space(text[start])) {
        start += 1;
    }

    while (start < text_length) {
        size_t end = std::min(start + chunk_size, text_length);

        if (end < text_length) {
            const size_t text_break = find_break(text, start, end, min_len);
            if (text_break != 0) {
                end = text_break; // otherwise no acceptable break exists: hard split
            }
        }

        size_t chunk_end = end;
        while (chunk_end > start && is_space(text[chunk_end - 1])) {
            chunk_end -= 1;
        }
        chunks.push_back(text.substr(start, chunk_end - start));

        if (end >= text_length) {
            break;
        }

        // Begin the next chunk `overlap` characters back.
        size_t next = (end > overlap) ? end - overlap : 0;
        if (next <= start) {
            next = end; // guarantees forward progress
        }

        // Prefer the earliest sentence start inside the overlap region.
        size_t sentence_start = end;
        for (size_t i = next; i < end; i += 1) {
            if (is_sentence_boundary(text, i)) {
                sentence_start = i;
                break;
            }
        }

        if (sentence_start < end) {
            next = sentence_start;
        } else {
            // No sentence start available: snap forward to the start of a word.
            while (next < end && !is_space(text[next - 1])) {
                next += 1;
            }
        }

        while (next < text_length && is_space(text[next])) {
            next += 1; // skip the whitespace
        }
        start = next;
    }

    return chunks;
}
