#include "ollama_client.hpp"
#include "user-interface.hpp"
#include <iostream>
#include <unistd.h>

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

    UserInterface ui = UserInterface();
    if (interactive == true) {
        printf("\nInteractive mode debug info.\n");
        printf("---\n");
        printf("isatty(0)=%d, isatty(1)=%d, isatty(2)=%d\n", isatty(0), isatty(1), isatty(2));
        printf("TERM=%s\n", getenv("TERM"));
        printf("TERMINFO=%s\n", getenv("TERMINFO") ? getenv("TERMINFO") : "(not set)");
        printf("LC_ALL=%s\n", getenv("LC_ALL") ? getenv("LC_ALL") : "(not set)");
        printf("LANG=%s\n", getenv("LANG") ? getenv("LANG") : "(not set)");
        printf("---\n");

        ui.run();
        ui.print_args(model, prompt, stream, interactive);

        while (prompt.length() == 0) {
            done = ui.next_prompt(&prompt);

            if (done == true) {
                ui.end_session(EXIT_SUCCESS);
            }
        }
    } else {
        UserInterface::print_args_noninteractive(model, prompt, stream, interactive);

        if (prompt.length() == 0) {
            done = UserInterface::next_prompt_noninteractive(&prompt);

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

    OllamaClient client = OllamaClient(&ui);

    client.query_model_list();

    struct OllamaPromptConfig config;
    config.model = model;
    config.prompt = prompt;
    config.stream = stream;

    client.query_model(config);

    if (interactive == true) {
        while (done == false) {
            done = ui.next_prompt(&prompt);
            if (done == true) {
                continue;
            }

            config.prompt = prompt;
            client.query_model(config);
            ui.request_redraw();
        }
        ui.end_session(EXIT_SUCCESS);
    } else {
        printf("\n🏁 The session has ended.\n");
    }

    return 0;
}
