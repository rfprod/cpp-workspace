#include <gtest/gtest.h>

#include <algorithm>
#include <initializer_list>
#include <iterator>
#include <string>
#include <utility>
#include <vector>

#include "utils/args_utils.hpp"

namespace {

// Owns the strings so the const char* pointers remain valid for the test.
class Argv {
  public:
    Argv(std::initializer_list<std::string> args = {}) : storage_{"app"} {
        storage_.insert(storage_.end(), args.begin(), args.end());
        ptrs_.reserve(storage_.size());
        std::transform(storage_.begin(), storage_.end(), std::back_inserter(ptrs_),
                       [](const std::string& s) { return s.c_str(); });
        argc_ = static_cast<int>(ptrs_.size());
    }
    Argv(const Argv&) = delete;
    Argv& operator=(const Argv&) = delete;

    int argc() const {
        return argc_;
    }
    const char* const* argv() const {
        return ptrs_.data();
    }

  private:
    std::vector<std::string> storage_;
    std::vector<const char*> ptrs_;
    int argc_ = 0;
};

} // namespace

TEST(ArgsUtilsDefaults, ReturnsDefaultsWhenNoArgsGiven) {
    ArgsUtils utils;
    Argv argv;
    auto p = utils.parse_args(argv.argc(), argv.argv());

    EXPECT_EQ(p.model, "gemma3n:latest");
    EXPECT_EQ(p.embedding_model, "embeddinggemma:latest");
    EXPECT_EQ(p.prompt, "Hey! What's up!");
    EXPECT_FALSE(p.stream);
    EXPECT_FALSE(p.rag);
    EXPECT_FALSE(p.interactive);
    EXPECT_FALSE(p.dark_theme);
    EXPECT_FALSE(p.skip_embedding);
    EXPECT_FALSE(p.help);
}

TEST(ArgsUtilsParse, AssignsStringValues) {
    ArgsUtils utils;
    Argv argv{"--model=llama3", "--embedding_model=nomic", "--prompt=hi there"};
    auto p = utils.parse_args(argv.argc(), argv.argv());

    EXPECT_EQ(p.model, "llama3");
    EXPECT_EQ(p.embedding_model, "nomic");
    EXPECT_EQ(p.prompt, "hi there");
}

TEST(ArgsUtilsParse, SplitsOnFirstEqualsOnly) {
    ArgsUtils utils;
    Argv argv{"--prompt=a=b"};
    EXPECT_EQ(utils.parse_args(argv.argc(), argv.argv()).prompt, "a=b");
}

TEST(ArgsUtilsParse, EmptyValueYieldsEmptyString) {
    ArgsUtils utils;
    Argv argv{"--model="};
    EXPECT_EQ(utils.parse_args(argv.argc(), argv.argv()).model, "");
}

TEST(ArgsUtilsParse, LastOccurrenceWins) {
    ArgsUtils utils;
    Argv argv{"--model=a", "--model=b"};
    EXPECT_EQ(utils.parse_args(argv.argc(), argv.argv()).model, "b");
}

TEST(ArgsUtilsParse, IgnoresArgsWithoutEquals) {
    ArgsUtils utils;
    Argv argv{"--stream", "--model"};
    auto p = utils.parse_args(argv.argc(), argv.argv());

    EXPECT_FALSE(p.stream);
    EXPECT_EQ(p.model, "gemma3n:latest");
}

TEST(ArgsUtilsParse, IgnoresUnknownKeys) {
    ArgsUtils utils;
    Argv argv{"--unknown=x"};
    auto p = utils.parse_args(argv.argc(), argv.argv());

    EXPECT_EQ(p.model, "gemma3n:latest");
    EXPECT_FALSE(p.stream);
}

using BoolField = bool ParsedArgs::*;
using BoolCase = std::pair<std::string, BoolField>;

class BoolFlagTest : public testing::TestWithParam<BoolCase> {};

TEST_P(BoolFlagTest, ExactTrueEnablesFlag) {
    ArgsUtils utils;
    Argv argv{GetParam().first + "=true"};
    auto p = utils.parse_args(argv.argc(), argv.argv());
    EXPECT_TRUE(p.*GetParam().second);
}

TEST_P(BoolFlagTest, NonExactTrueValuesLeaveFlagDisabled) {
    ArgsUtils utils;
    for (const char* val : {"TRUE", "1", "yes", "false"}) {
        Argv argv{GetParam().first + "=" + val};
        auto p = utils.parse_args(argv.argc(), argv.argv());
        EXPECT_FALSE(p.*GetParam().second) << "value: " << val;
    }
}

INSTANTIATE_TEST_SUITE_P(AllBoolFlags, BoolFlagTest,
                         testing::Values(BoolCase{"--stream", &ParsedArgs::stream}, BoolCase{"--rag", &ParsedArgs::rag},
                                         BoolCase{"--interactive", &ParsedArgs::interactive},
                                         BoolCase{"--dark_theme", &ParsedArgs::dark_theme},
                                         BoolCase{"--skip_embedding", &ParsedArgs::skip_embedding},
                                         BoolCase{"--help", &ParsedArgs::help}));

TEST(ArgsUtilsHelp, DocumentsEveryHandledFlag) {
    ArgsUtils utils;
    const std::string text = utils.help_text();
    for (const char* key : {"--model=", "--embedding_model=", "--prompt=", "--stream=", "--rag=", "--interactive=",
                            "--dark_theme=", "--skip_embedding=", "--help="}) {
        EXPECT_NE(text.find(key), std::string::npos) << "missing: " << key;
    }
}
