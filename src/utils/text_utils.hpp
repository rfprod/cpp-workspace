#pragma once

#include <cstddef>
#include <string>
#include <vector>

/**
 * @class TextUtils
 * @brief Utility class for text manipulation operations.
 *
 * Provides static helpers for wrapping text to a maximum line width and for
 * splitting text into overlapping, sentence-aware chunks suitable for
 * vector embeddings.
 */
class TextUtils {
  private:
    /**
     * Abbreviations that end with a period but do not end a sentence.
     * Entries are lowercase and include the trailing period.
     */
    static const std::vector<std::string> abbreviationExceptions;

    /**
     * Checks whether a character is whitespace.
     *
     * @param c The character to test.
     * @return true if c is a space, tab, newline, or other whitespace character.
     */
    static bool is_space(const char c);

    /**
     * Checks whether the token ending before a whitespace character is an abbreviation.
     *
     * @param text The full input text.
     * @param space_idx The index of the whitespace character that follows the token.
     * @return true if the token (case-insensitive) is in abbreviationExceptions.
     */
    static bool is_abbreviation(const std::string& text, const size_t space_idx);

    /**
     * Checks whether a whitespace character marks the end of a sentence.
     *
     * A sentence ends at whitespace preceded by '!' or '?', or by '.' when
     * the preceding token is not an abbreviation.
     *
     * @param text The full input text.
     * @param i The index to test. Must be less than text.length().
     * @return true if index i is a sentence boundary.
     */
    static bool is_sentence_boundary(const std::string& text, const size_t i);

    /**
     * Finds the best position to end a chunk within a window.
     *
     * Searches backward from end for a sentence boundary, then for any
     * whitespace. Only positions at or after start + min_len are considered,
     * so the resulting chunk is at least min_len characters long.
     *
     * @param text The full input text.
     * @param start The index where the current chunk begins.
     * @param end The maximum index for the chunk end. Must be less than text.length().
     * @param min_len The minimum chunk length in characters.
     * @return The break index, or 0 if no suitable break exists.
     */
    static size_t find_break(const std::string& text, const size_t start, const size_t end, const size_t min_len);

  public:
    /**
     * Wraps text to fit within a maximum line width.
     *
     * Breaks lines at the last space within the width. If a word is longer than
     * the width, the line is broken at max_width characters.
     *
     * @param text The input text to wrap.
     * @param max_width The maximum number of characters per line. Must be greater than 0.
     * @return A vector of wrapped lines. Each line is at most max_width characters.
     */
    static std::vector<std::string> wrap_text(const std::string& text, const int max_width);

    /**
     * Splits text into sentence-aware chunks with optional overlap.
     *
     * @param text The input text to split.
     * @param chunk_size The maximum size of each chunk in characters. Must be greater than 0.
     * @param overlap The approximate number of characters shared between consecutive chunks.
     *                Must be less than chunk_size. Defaults to 0.
     * @return A vector of chunks with leading and trailing whitespace removed.
     */
    static std::vector<std::string> split_text(const std::string& text, const size_t chunk_size,
                                               const size_t overlap = 0);
};
