#include "args_utils.hpp"
#include "utils/exit_request.hpp"

/**
 * Initializes the ArgsUtils instance with command-line argument handlers.
 * Sets up a map of argument keys to handler functions that parse and assign values to ParsedArgs.
 */
ArgsUtils::ArgsUtils() {
    this->handlers = {
        {"--model", [&](const std::string& val, struct ParsedArgs* args) { args->model = val; }},
        {"--embedding_model", [&](const std::string& val, struct ParsedArgs* args) { args->embedding_model = val; }},
        {"--prompt", [&](const std::string& val, struct ParsedArgs* args) { args->prompt = val; }},
        {"--stream", [&](const std::string& val, struct ParsedArgs* args) { args->stream = val == "true"; }},
        {"--rag", [&](const std::string& val, struct ParsedArgs* args) { args->rag = val == "true"; }},
        {"--interactive", [&](const std::string& val, struct ParsedArgs* args) { args->interactive = val == "true"; }},
        {"--dark_theme", [&](const std::string& val, struct ParsedArgs* args) { args->dark_theme = val == "true"; }},
        {"--skip_embedding",
         [&](const std::string& val, struct ParsedArgs* args) { args->skip_embedding = val == "true"; }},
        {"--help", [&](const std::string& val, struct ParsedArgs* args) { args->help = val == "true"; }},
    };
}

/**
 * Parses command-line arguments and returns a ParsedArgs struct.
 * Initializes all arguments with default values, then overrides them with values from the command line.
 * Expects arguments in the format --key=value.
 *
 * @param argc Pointer to argument count from main()
 * @param argv Array of argument strings from main()
 * @return ParsedArgs struct containing all parsed argument values
 */
struct ParsedArgs ArgsUtils::parse_args(const int argc, const char* const argv[]) const {
    struct ParsedArgs parsed_args {};
    parsed_args.model = "gemma3n:latest";
    parsed_args.embedding_model = "embeddinggemma:latest";
    parsed_args.prompt = "Hey! What's up!";
    parsed_args.stream = false;
    parsed_args.rag = false;
    parsed_args.interactive = false;
    parsed_args.dark_theme = false;
    parsed_args.skip_embedding = false;
    parsed_args.help = false;

    std::string arg{};
    for (int i = 1; i < argc; i += 1) {
        arg = argv[i];
        size_t eq_pos = arg.find('=');

        if (eq_pos == std::string::npos) {
            continue;
        }

        auto it = handlers.find(arg.substr(0, eq_pos));
        if (it != handlers.end()) {
            it->second(arg.substr(eq_pos + 1), &parsed_args);
        }
    }

    return parsed_args;
}

/**
 * Returns the help text containing information about all supported runtime arguments.
 */
const char* ArgsUtils::help_text() {
    return "Usage: app [OPTIONS]\n\n"
           "Options:\n"
           "  --model=<name>                The LLM model to use for inference.\n"
           "                                (default: gemma3n:latest)\n\n"
           "  --embedding_model=<name>      The model to use for generating embeddings/\n"
           "                                (default: embeddinggemma:latest)\n\n"
           "  --prompt=<text>               Initial prompt or query to send to the model.\n"
           "                                (default: Hey! What's up!)\n\n"
           "  --stream=<true|false>         Enable streaming responses.\n"
           "                                (default: false)\n\n"
           "  --rag=<true|false>            Enable Retrieval-Augmented Generation (RAG) mode.\n"
           "                                (default: false)\n\n"
           "  --interactive=<true|false>    Enable interactive mode for multi-turn conversations.\n"
           "                                (default: false)\n\n"
           "  --dark_theme=<true|false>     Use dark theme for the ncurses interface.\n"
           "                                (default: false)\n\n"
           "  --skip_embedding=<true|false> Skip embedding creation.\n"
           "                                (default: false)\n\n"
           "  --help=<true|false>           Print help.\n"
           "                                (default: false)\n";
}

/**
 * Prints usage information for all supported runtime arguments and exits.
 * Displays each argument name, type, description, and default value.
 */
void ArgsUtils::print_help() {
    printf("%s", this->help_text());

    std::exit(EXIT_SUCCESS);
}
