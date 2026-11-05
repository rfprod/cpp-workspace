#include <gtest/gtest.h>

#include <string>
#include <utility>
#include <vector>

#include "utils/text_utils.hpp"

TEST(TextUtils_wrap_text, no_wrap) {
    std::string text = "This text will not be wrapped.";
    int max_width = text.length() + 1;

    std::vector<std::string> result = TextUtils::wrap_text(text, max_width);

    EXPECT_EQ(result.size(), 1);
    EXPECT_EQ(result.at(0), text);
}

TEST(TextUtils_wrap_text, wrap) {
    std::string text = "This text value will be wrapped.";
    int max_width = text.length() / 2;

    std::vector<std::string> result = TextUtils::wrap_text(text, max_width);

    size_t last_non_space{};

    std::string first = text.substr(0, max_width);
    last_non_space = first.find_last_not_of(' ');
    first.resize(last_non_space + 1);

    std::string second = text.substr(max_width);
    last_non_space = second.find_last_not_of(' ');
    second.resize(last_non_space + 1);

    EXPECT_EQ(result.size(), 2);
    EXPECT_EQ(result.at(0), first);
    EXPECT_EQ(result.at(1), second);
}

TEST(TextUtils_split_text, no_split) {
    std::string text = "This text will not be chunked.";
    size_t chunk_size = text.length() + 1;
    size_t overlap = 0;

    std::vector<std::string> chunks = TextUtils::split_text(text, chunk_size, overlap);

    EXPECT_EQ(chunks.size(), 1);
    EXPECT_EQ(chunks.at(0), text);
}

TEST(TextUtils_split_text, split_no_overlap) {
    std::string text = "This text value will be chunked.";
    size_t chunk_size = text.length() / 2;
    size_t overlap = 0;

    std::vector<std::string> chunks = TextUtils::split_text(text, chunk_size, overlap);

    size_t last_non_space{};

    std::string first = text.substr(0, chunk_size);
    last_non_space = first.find_last_not_of(' ');
    first.resize(last_non_space + 1);

    std::string second = text.substr(chunk_size - overlap);
    last_non_space = second.find_last_not_of(' ');
    second.resize(last_non_space + 1);

    EXPECT_EQ(chunks.size(), 2);
    EXPECT_EQ(chunks.at(0), first);
    EXPECT_EQ(chunks.at(1), second);
}

TEST(TextUtils_split_text, split_with_overlap) {
    std::string text = "This text value is chunked with overlap.";
    size_t chunk_size = text.length() / 2;
    size_t overlap = 4;

    std::vector<std::string> chunks = TextUtils::split_text(text, chunk_size, overlap);

    EXPECT_EQ(chunks.size(), 3);
}
