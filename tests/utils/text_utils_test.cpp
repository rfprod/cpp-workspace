#include <gtest/gtest.h>

#include <algorithm>
#include <climits>
#include <cstddef>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

#include "utils/text_utils.hpp"

TEST(TextUtils_wrap_text, no_wrap) {
    const std::string text = "This text will not be wrapped.";
    const int max_width = static_cast<int>(text.length()) + 1;

    const std::vector<std::string> result = TextUtils::wrap_text(text, max_width);

    ASSERT_EQ(result.size(), 1u);
    EXPECT_EQ(result[0], text);
}

TEST(TextUtils_wrap_text, wrap) {
    const std::string text = "This text value will be wrapped.";
    const int max_width = 16;

    const std::vector<std::string> result = TextUtils::wrap_text(text, max_width);

    ASSERT_EQ(result.size(), 2u);
    EXPECT_EQ(result[0], "This text value");
    EXPECT_EQ(result[1], "will be wrapped.");
}

/**
 * Argument validation.
 */

TEST(TextUtils_wrap_text, throws_on_zero_width) {
    EXPECT_THROW(TextUtils::wrap_text("abc", 0), std::invalid_argument);
}

TEST(TextUtils_wrap_text, throws_on_negative_width) {
    EXPECT_THROW(TextUtils::wrap_text("abc", -1), std::invalid_argument);
    EXPECT_THROW(TextUtils::wrap_text("abc", INT_MIN), std::invalid_argument);
}

/**
 * Empty and whitespace-only input (covers the first == npos branch).
 */

TEST(TextUtils_wrap_text, empty_input_returns_no_lines) {
    EXPECT_TRUE(TextUtils::wrap_text("", 10).empty());
}

TEST(TextUtils_wrap_text, only_spaces_returns_no_lines) {
    EXPECT_TRUE(TextUtils::wrap_text("       ", 3).empty());
}

TEST(TextUtils_wrap_text, leading_spaces_are_removed) {
    const std::vector<std::string> result = TextUtils::wrap_text("   hello world", 20);

    ASSERT_EQ(result.size(), 1u);
    EXPECT_EQ(result[0], "hello world");
}

/**
 * Width boundaries.
 */

TEST(TextUtils_wrap_text, text_exactly_max_width_is_not_wrapped) {
    const std::vector<std::string> result = TextUtils::wrap_text("abcdefghij", 10);

    ASSERT_EQ(result.size(), 1u);
    EXPECT_EQ(result[0], "abcdefghij");
}

TEST(TextUtils_wrap_text, width_one_splits_every_character) {
    const std::vector<std::string> result = TextUtils::wrap_text("abc", 1);

    ASSERT_EQ(result.size(), 3u);
    EXPECT_EQ(result[0], "a");
    EXPECT_EQ(result[1], "b");
    EXPECT_EQ(result[2], "c");
}

TEST(TextUtils_wrap_text, space_exactly_at_width_is_the_break_point) {
    const std::vector<std::string> result = TextUtils::wrap_text("abcd efgh", 4);

    ASSERT_EQ(result.size(), 2u);
    EXPECT_EQ(result[0], "abcd");
    EXPECT_EQ(result[1], "efgh");
}

/**
 * Hard breaks (covers the wrap_position == npos branch).
 */

TEST(TextUtils_wrap_text, word_longer_than_width_is_split) {
    const std::vector<std::string> result = TextUtils::wrap_text("abcdefghij", 4);

    ASSERT_EQ(result.size(), 3u);
    EXPECT_EQ(result[0], "abcd");
    EXPECT_EQ(result[1], "efgh");
    EXPECT_EQ(result[2], "ij");
}

TEST(TextUtils_wrap_text, long_word_followed_by_space_is_hard_broken) {
    const std::vector<std::string> result = TextUtils::wrap_text("abcde fgh", 4);

    ASSERT_EQ(result.size(), 3u);
    EXPECT_EQ(result[0], "abcd");
    EXPECT_EQ(result[1], "e");
    EXPECT_EQ(result[2], "fgh");
}

/**
 * Whitespace handling around breaks (covers the next_start == npos branch).
 */

TEST(TextUtils_wrap_text, multiple_spaces_at_break_are_collapsed) {
    const std::vector<std::string> result = TextUtils::wrap_text("aaa   bbb", 5);

    ASSERT_EQ(result.size(), 2u);
    EXPECT_EQ(result[0], "aaa");
    EXPECT_EQ(result[1], "bbb");
}

TEST(TextUtils_wrap_text, trailing_spaces_after_last_break_produce_no_extra_line) {
    const std::vector<std::string> result = TextUtils::wrap_text("abcde     ", 5);

    ASSERT_EQ(result.size(), 1u);
    EXPECT_EQ(result[0], "abcde");
}

TEST(TextUtils_wrap_text, trailing_spaces_on_unwrapped_text_are_removed) {
    const std::vector<std::string> result = TextUtils::wrap_text("hello   ", 20);

    ASSERT_EQ(result.size(), 1u);
    EXPECT_EQ(result[0], "hello");
}

/**
 * Multi-line behaviour.
 */

TEST(TextUtils_wrap_text, multiple_lines_with_hard_coded_expectations) {
    const std::vector<std::string> result = TextUtils::wrap_text("The quick brown fox jumps over the lazy dog", 10);

    const std::vector<std::string> expected = {"The quick", "brown fox", "jumps over", "the lazy", "dog"};
    EXPECT_EQ(result, expected);
}

TEST(TextUtils_wrap_text, every_line_respects_width_and_has_no_edge_spaces) {
    const std::string text = "The quick brown fox jumps over the lazy dog";

    for (int width = 1; width <= static_cast<int>(text.length()) + 2; width += 1) {
        const std::vector<std::string> result = TextUtils::wrap_text(text, width);

        for (const std::string& line : result) {
            SCOPED_TRACE("width=" + std::to_string(width) + " line=\"" + line + "\"");
            EXPECT_FALSE(line.empty());
            EXPECT_LE(line.length(), static_cast<size_t>(width));
            EXPECT_NE(line.front(), ' ');
            EXPECT_NE(line.back(), ' ');
        }
    }
}

TEST(TextUtils_wrap_text, rejoined_lines_reproduce_original_words) {
    const std::string text = "The quick brown fox jumps over the lazy dog";
    const int longest_word = 5;

    for (int width = longest_word; width <= static_cast<int>(text.length()); width += 1) {
        const std::vector<std::string> result = TextUtils::wrap_text(text, width);

        std::string rejoined;
        for (size_t i = 0; i < result.size(); ++i) {
            if (i > 0) {
                rejoined += ' ';
            }
            rejoined += result[i];
        }

        SCOPED_TRACE("width=" + std::to_string(width));
        EXPECT_EQ(rejoined, text);
    }
}

/**
 * Characters that are not spaces.
 */

TEST(TextUtils_wrap_text, tabs_are_not_break_points) {
    const std::vector<std::string> result = TextUtils::wrap_text("abc\tdef", 4);

    ASSERT_EQ(result.size(), 2u);
    EXPECT_EQ(result[0], "abc\t");
    EXPECT_EQ(result[1], "def");
}

/**
 * Large input (smoke test for correctness and for the O(n^2 / width) copying).
 */

TEST(TextUtils_wrap_text, large_input_wraps_into_expected_number_of_lines) {
    constexpr size_t word_count = 10000;

    std::ostringstream builder;
    for (size_t i = 0; i < word_count; i += 1) {
        if (i > 0) {
            builder << ' ';
        }
        builder << "ab";
    }

    // Each line holds three "ab" words (8 characters); the width of 10 cannot fit a fourth.
    // 10000 words yield 3333 full lines plus one final line containing a single word.
    const std::vector<std::string> result = TextUtils::wrap_text(builder.str(), 10);

    ASSERT_EQ(result.size(), 3334u);
    EXPECT_EQ(result.front(), "ab ab ab");
    EXPECT_EQ(result.back(), "ab");
}

// split_text

TEST(TextUtils_split_text, no_split) {
    const std::string text = "This text will not be chunked.";
    const auto chunks = TextUtils::split_text(text, text.length() + 1, 0);

    ASSERT_EQ(chunks.size(), 1u);
    EXPECT_EQ(chunks[0], text);
}

TEST(TextUtils_split_text, split_no_overlap) {
    const std::string text = "This text value will be chunked.";
    const auto chunks = TextUtils::split_text(text, 16, 0);

    ASSERT_EQ(chunks.size(), 2u);
    EXPECT_EQ(chunks[0], "This text value");
    EXPECT_EQ(chunks[1], "will be chunked.");
}

TEST(TextUtils_split_text, split_with_overlap) {
    const std::string text = "This text value is chunked with overlap.";
    const auto chunks = TextUtils::split_text(text, 20, 4);

    ASSERT_EQ(chunks.size(), 3u);
    EXPECT_EQ(chunks[0], "This text value is");
    EXPECT_EQ(chunks[1], "is chunked with");
    EXPECT_EQ(chunks[2], "with overlap.");
}

TEST(TextUtils_split_text, empty_input_returns_no_chunks) {
    EXPECT_TRUE(TextUtils::split_text("", 10, 0).empty());
}

TEST(TextUtils_split_text, whitespace_only_input_returns_no_chunks) {
    EXPECT_TRUE(TextUtils::split_text("   \t\n  ", 10, 0).empty());
}

TEST(TextUtils_split_text, leading_whitespace_is_trimmed) {
    const auto chunks = TextUtils::split_text("   Hello world", 100, 0);

    ASSERT_EQ(chunks.size(), 1u);
    EXPECT_EQ(chunks[0], "Hello world");
}

TEST(TextUtils_split_text, trailing_whitespace_is_trimmed) {
    const auto chunks = TextUtils::split_text("Hello world   ", 100, 0);

    ASSERT_EQ(chunks.size(), 1u);
    EXPECT_EQ(chunks[0], "Hello world");
}

TEST(TextUtils_split_text, text_exactly_chunk_size_is_one_chunk) {
    const auto chunks = TextUtils::split_text("abcdefghij", 10, 0);

    ASSERT_EQ(chunks.size(), 1u);
    EXPECT_EQ(chunks[0], "abcdefghij");
}

TEST(TextUtils_split_text, zero_chunk_size_throws) {
    EXPECT_THROW(TextUtils::split_text("abc", 0, 0), std::invalid_argument);
}

TEST(TextUtils_split_text, overlap_equal_to_chunk_size_throws) {
    EXPECT_THROW(TextUtils::split_text("abc", 5, 5), std::invalid_argument);
}

TEST(TextUtils_split_text, overlap_greater_than_chunk_size_throws) {
    EXPECT_THROW(TextUtils::split_text("abc", 5, 6), std::invalid_argument);
}

TEST(TextUtils_split_text, hard_split_when_no_whitespace) {
    const auto chunks = TextUtils::split_text("abcdefghijklmnopqrstuvwxyz", 10, 0);

    ASSERT_EQ(chunks.size(), 3u);
    EXPECT_EQ(chunks[0], "abcdefghij");
    EXPECT_EQ(chunks[1], "klmnopqrst");
    EXPECT_EQ(chunks[2], "uvwxyz");
}

TEST(TextUtils_split_text, overlap_honored_when_no_whitespace) {
    const auto chunks = TextUtils::split_text("abcdefghijklmnopqrstuvwxyz", 10, 3);

    ASSERT_EQ(chunks.size(), 4u);
    EXPECT_EQ(chunks[0], "abcdefghij");
    EXPECT_EQ(chunks[1], "hijklmnopq");
    EXPECT_EQ(chunks[2], "opqrstuvwx");
    EXPECT_EQ(chunks[3], "vwxyz");
}

TEST(TextUtils_split_text, prefers_sentence_boundary) {
    const auto chunks = TextUtils::split_text("First sentence is here. Second one follows now.", 30, 0);

    ASSERT_EQ(chunks.size(), 2u);
    EXPECT_EQ(chunks[0], "First sentence is here.");
    EXPECT_EQ(chunks[1], "Second one follows now.");
}

TEST(TextUtils_split_text, abbreviation_is_not_a_sentence_boundary) {
    const auto chunks = TextUtils::split_text("Call Dr. Lee now please.", 12, 0);

    ASSERT_EQ(chunks.size(), 2u);
    EXPECT_EQ(chunks[0], "Call Dr. Lee");
    EXPECT_EQ(chunks[1], "now please.");
}

TEST(TextUtils_split_text, no_chunk_exceeds_chunk_size) {
    const std::string text = "The quick brown fox. Jumps over the lazy dog! Does it work? Yes, it does.";

    for (size_t chunk_size = 1; chunk_size <= 20; ++chunk_size) {
        for (size_t overlap = 0; overlap < chunk_size; ++overlap) {
            const auto chunks = TextUtils::split_text(text, chunk_size, overlap);
            ASSERT_FALSE(chunks.empty());

            for (const auto& chunk : chunks) {
                EXPECT_FALSE(chunk.empty());
                EXPECT_LE(chunk.size(), chunk_size) << "chunk_size=" << chunk_size << " overlap=" << overlap;
                EXPECT_NE(chunk.front(), ' ');
                EXPECT_NE(chunk.back(), ' ');
            }
        }
    }
}

namespace {

std::string remove_spaces(std::string s) {
    s.erase(std::remove(s.begin(), s.end(), ' '), s.end());
    return s;
}

} // namespace

TEST(TextUtils_split_text, no_overlap_keeps_every_non_space_character_once) {
    const std::string text = "The quick brown fox jumps over the lazy dog and keeps running far away.";
    const std::string expected = remove_spaces(text);

    for (size_t chunk_size = 1; chunk_size <= 25; ++chunk_size) {
        const auto chunks = TextUtils::split_text(text, chunk_size, 0);

        std::string joined;
        for (const auto& chunk : chunks) {
            joined += chunk;
        }

        EXPECT_EQ(remove_spaces(joined), expected) << "chunk_size=" << chunk_size;
    }
}
