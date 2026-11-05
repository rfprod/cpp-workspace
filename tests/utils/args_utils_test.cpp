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

TEST(ArgsUtils_defaults, returns_defaults_when_no_args_given) {
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

TEST(ArgsUtils_parse_args, assigns_string_values) {
    ArgsUtils utils;
    Argv argv{"--model=llama3", "--embedding_model=nomic", "--prompt=hi there"};
    auto p = utils.parse_args(argv.argc(), argv.argv());

    EXPECT_EQ(p.model, "llama3");
    EXPECT_EQ(p.embedding_model, "nomic");
    EXPECT_EQ(p.prompt, "hi there");
}

TEST(ArgsUtils_parse_args, splits_on_first_equals_only) {
    ArgsUtils utils;
    Argv argv{"--prompt=a=b"};
    EXPECT_EQ(utils.parse_args(argv.argc(), argv.argv()).prompt, "a=b");
}

TEST(ArgsUtils_parse_args, empty_value_yields_empty_string) {
    ArgsUtils utils;
    Argv argv{"--model="};
    EXPECT_EQ(utils.parse_args(argv.argc(), argv.argv()).model, "");
}

TEST(ArgsUtils_parse_args, last_occurrence_wins) {
    ArgsUtils utils;
    Argv argv{"--model=a", "--model=b"};
    EXPECT_EQ(utils.parse_args(argv.argc(), argv.argv()).model, "b");
}

TEST(ArgsUtils_parse_args, ignores_args_without_equals) {
    ArgsUtils utils;
    Argv argv{"--stream", "--model"};
    auto p = utils.parse_args(argv.argc(), argv.argv());

    EXPECT_FALSE(p.stream);
    EXPECT_EQ(p.model, "gemma3n:latest");
}

TEST(ArgsUtils_parse_args, ignores_unknown_keys) {
    ArgsUtils utils;
    Argv argv{"--unknown=x"};
    auto p = utils.parse_args(argv.argc(), argv.argv());

    EXPECT_EQ(p.model, "gemma3n:latest");
    EXPECT_FALSE(p.stream);
}

using BoolField = bool ParsedArgs::*;
using BoolCase = std::pair<std::string, BoolField>;

class BoolFlagTest : public testing::TestWithParam<BoolCase> {};

TEST_P(BoolFlagTest, exact_true_enables_flag) {
    ArgsUtils utils;
    Argv argv{GetParam().first + "=true"};
    auto p = utils.parse_args(argv.argc(), argv.argv());
    EXPECT_TRUE(p.*GetParam().second);
}

TEST_P(BoolFlagTest, non_exact_true_values_leave_flag_disabled) {
    ArgsUtils utils;
    for (const char* val : {"TRUE", "1", "yes", "false"}) {
        Argv argv{GetParam().first + "=" + val};
        auto p = utils.parse_args(argv.argc(), argv.argv());
        EXPECT_FALSE(p.*GetParam().second) << "value: " << val;
    }
}

INSTANTIATE_TEST_SUITE_P(AllBool_flags, BoolFlagTest,
                         testing::Values(BoolCase{"--stream", &ParsedArgs::stream}, BoolCase{"--rag", &ParsedArgs::rag},
                                         BoolCase{"--interactive", &ParsedArgs::interactive},
                                         BoolCase{"--dark_theme", &ParsedArgs::dark_theme},
                                         BoolCase{"--skip_embedding", &ParsedArgs::skip_embedding},
                                         BoolCase{"--help", &ParsedArgs::help}));

TEST(ArgsUtils_help_text, documents_every_handled_flag) {
    ArgsUtils utils;
    const std::string text = utils.help_text();
    for (const char* key : {"--model=", "--embedding_model=", "--prompt=", "--stream=", "--rag=", "--interactive=",
                            "--dark_theme=", "--skip_embedding=", "--help="}) {
        EXPECT_NE(text.find(key), std::string::npos) << "missing: " << key;
    }
}
