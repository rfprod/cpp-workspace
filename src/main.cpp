#include "ollama_client.hpp"
#include <iostream>

bool next_prompt(std::string* prompt) {
    bool done = false;
    printf("⏳ Type your prompt (q/Q to quit): ");
    if (!std::getline(std::cin, *prompt, '\n')) {
        fprintf(stderr, "Error reading user input.\n");
        exit(1);
    }

    if (*prompt == "q" || *prompt == "Q") {
        done = true;
    }

    return done;
}

int main(int argc, const char* argv[]) {
    std::string model = "gemma3n:latest";
    std::string prompt = "Hey! What's up!";
    bool stream = false;
    bool interactive = false;

    bool done = false;

    std::string arg{};
    for (int i = 1; i < argc; i += 1) {
        arg = argv[i];
        if (arg.starts_with("--model=") == true) {
            model = arg.substr(8);
        } else if (arg.starts_with("--prompt=") == true) {
            prompt = arg.substr(9);
        } else if (arg.starts_with("--stream=") == true) {
            stream = arg.substr(9) == "true" ? true : false;
        } else if (arg.starts_with("--interactive=") == true) {
            interactive = arg.substr(14) == "true" ? true : false;
        }
    }

    printf("\n");
    printf("🛈 model = %s\n", model.c_str());
    printf("🛈 prompt = %s\n", prompt.c_str());
    printf("🛈 stream = %s\n", stream ? "true" : "false");
    printf("🛈 interactive = %s\n", interactive ? "true" : "false");
    printf("\n");

    if (interactive == true) {
        while (prompt.length() == 0) {
            done = next_prompt(&prompt);

            if (done == true) {
                printf("\n🏁 The session has ended.\n");
                exit(0);
            }
        }
    } else {
        if (prompt.length() == 0) {
            done = next_prompt(&prompt);

            if (prompt.length() == 0) {
                printf("\n🏁 Empty prompt. The session has ended.\n");
                exit(0);
            }

            if (done == true) {
                printf("\n🏁 The session has ended.\n");
                exit(0);
            }
        }
    }

    OllamaClient client = OllamaClient();

    client.query_model_list();

    struct OllamaPromptConfig config;
    config.model = model;
    config.prompt = prompt;
    config.stream = stream;

    client.query_model(config);

    if (interactive == true) {
        while (done == false) {
            done = next_prompt(&prompt);
            if (done == true) {
                continue;
            }

            config.prompt = prompt;
            client.query_model(config);
        }
    }

    printf("\n🏁 The session has ended.\n");

    return 0;
}
