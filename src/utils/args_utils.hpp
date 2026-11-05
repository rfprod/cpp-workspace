#pragma once

#include <functional>
#include <string>
#include <unordered_map>

/**
 * Represents parsed command-line arguments.
 * Contains all supported runtime arguments with their parsed values.
 * Boolean fields default to false; string fields default to empty strings.
 */
struct ParsedArgs {
    std::string model;
    std::string embedding_model;
    std::string prompt;
    bool stream;
    bool rag;
    bool interactive;
    bool ncurses;
    bool dark_theme;
    bool skip_embedding;
    bool help;
};

/**
 * Utility class for parsing command-line arguments.
 * Uses a handler map to associate argument keys with parsing functions,
 * enabling O(1) lookup and flexible argument handling.
 */
class ArgsUtils {
  private:
    /**
     * Maps command-line argument keys to handler functions.
     * Each handler processes the argument value and updates the corresponding field in ParsedArgs.
     * Provides O(1) lookup when parsing arguments.
     */
    std::unordered_map<std::string, std::function<void(const std::string&, struct ParsedArgs* args)>> handlers;

  public:
    ArgsUtils();

    struct ParsedArgs parse_args(const int argc, const char* const argv[]) const;

    const char* help_text();

    void print_help();
};
