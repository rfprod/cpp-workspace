#include "interface/user_interface.hpp"
#include "ollama/ollama_client.hpp"
#include "storage/qdrant_client.hpp"
#include "utils/curl_utils.hpp"
#include "utils/embedding_utils.hpp"
#include "utils/retrieving_utils.hpp"
#include "utils/system_utils.hpp"
#include "utils/text_utils.hpp"
#include <iostream>
#include <unistd.h>
#include <vector>

int main(int argc, const char* argv[]) {
    std::string model = "gemma3n:latest";
    std::string embedding_model = "embeddinggemma:latest";
    std::string prompt = "Hey! What's up!";
    bool stream = false;
    bool rag = false;
    bool interactive = false;
    bool dark_theme = true;
    bool skip_embedding = false;

    bool done = false;
    std::string message{};

    std::string arg{};
    for (int i = 1; i < argc; i += 1) {
        arg = argv[i];
        if (arg.starts_with("--model=") == true) {
            model = arg.substr(8);
        } else if (arg.starts_with("--embedding_model=") == true) {
            embedding_model = arg.substr(18);
        } else if (arg.starts_with("--prompt=") == true) {
            prompt = arg.substr(9);
        } else if (arg.starts_with("--stream=") == true) {
            stream = arg.substr(9) == "true" ? true : false;
        } else if (arg.starts_with("--rag=") == true) {
            rag = arg.substr(6) == "true" ? true : false;
        } else if (arg.starts_with("--interactive=") == true) {
            interactive = arg.substr(14) == "true" ? true : false;
        } else if (arg.starts_with("--dark_theme=") == true) {
            dark_theme = arg.substr(13) == "true" ? true : false;
        } else if (arg.starts_with("--skip_embedding=") == true) {
            skip_embedding = arg.substr(17) == "true" ? true : false;
        }
    }

    UserInterface ui(dark_theme);
    if (interactive == true) {
        message = std::format("\nInteractive mode debug info.\n"
                              "---\n"
                              "isatty(0)={}, isatty(1)={}, isatty(2)={}\n"
                              "TERM={}\n"
                              "TERMINFO={}\n"
                              "LC_ALL={}\n"
                              "LANG={}\n"
                              "---\n",
                              isatty(0), isatty(1), isatty(2), getenv("TERM") ? getenv("TERM") : "(not set)",
                              getenv("TERMINFO") ? getenv("TERMINFO") : "(not set)",
                              getenv("LC_ALL") ? getenv("LC_ALL") : "(not set)",
                              getenv("LANG") ? getenv("LANG") : "(not set)");

        UserInterface::print_message_noninteractive(message);

        ui.run();
        ui.print_args(model, embedding_model, prompt, stream, rag, interactive, dark_theme, skip_embedding);

        while (prompt.length() == 0) {
            done = ui.next_prompt(&prompt);

            if (done == true) {
                ui.end_session(EXIT_SUCCESS);
            }
        }
    } else {
        UserInterface::print_args_noninteractive(model, embedding_model, prompt, stream, rag, interactive, dark_theme,
                                                 skip_embedding);

        if (prompt.length() == 0) {
            done = UserInterface::next_prompt_noninteractive(&prompt);

            if (prompt.length() == 0) {
                UserInterface::print_message_noninteractive("\n🏁 Empty prompt. The session has ended.\n");
                exit(0);
            }

            if (done == true) {
                UserInterface::print_message_noninteractive("\n🏁 The session has ended.\n");
                exit(0);
            }
        }
    }

    CurlUtils curl(&ui);

    SystemUtils system_utils(&ui);

    QdrantClient storage_client(&ui, &curl, &system_utils);

    struct OllamaClientConfig ollama_config {};

    OllamaClient ollama_client(&ollama_config, &ui, &curl, &system_utils);

    EmbeddingUtils embedding_utils(&ui, &curl, &ollama_client, &storage_client);

    struct RetrievingUtilsConfig retrieving_config {};

    RetrievingUtils retrieving_utils(&retrieving_config, &ui, &curl, &ollama_client, &storage_client);

    std::vector<std::string> models = ollama_client.query_model_list();
    bool force_model_selection = true;
    for (size_t i = 0; i < models.size(); i += 1) {
        if (models[i] == model) {
            force_model_selection = false;
        }
    }

    if (force_model_selection == true) {
        message = "⚠ A Language model is not selected.\n";
        if (interactive == true) {
            ui.print_message(message);
            ui.print_model_list(models);
            ui.select_model(models, &model);
            ui.print_args(model, embedding_model, prompt, stream, rag, interactive, dark_theme, skip_embedding);
        } else {
            UserInterface::print_message_noninteractive(message);
            UserInterface::print_model_list_noninteractive(models);
            UserInterface::select_model_noninteractive(models, &model);
            UserInterface::print_args_noninteractive(model, embedding_model, prompt, stream, rag, interactive,
                                                     dark_theme, skip_embedding);
        }
    }

    if (skip_embedding == false) {
        bool force_embedding_model_selection = true;
        for (size_t i = 0; i < models.size(); i += 1) {
            if (models[i] == embedding_model) {
                force_embedding_model_selection = false;
            }
        }
        if (force_embedding_model_selection == true) {
            message = "⚠ An Embedding model is not selected.\n";
            if (interactive == true) {
                ui.print_message(message);
                ui.print_model_list(models);
                ui.select_model(models, &embedding_model);
                ui.print_args(model, embedding_model, prompt, stream, rag, interactive, dark_theme, skip_embedding);
            } else {
                UserInterface::print_message_noninteractive(message);
                UserInterface::print_model_list_noninteractive(models);
                UserInterface::select_model_noninteractive(models, &embedding_model);
                UserInterface::print_args_noninteractive(model, embedding_model, prompt, stream, rag, interactive,
                                                         dark_theme, skip_embedding);
            }
        }
    }

    ollama_config.model = model;
    ollama_config.embedding_model = embedding_model;
    ollama_config.prompt = prompt;
    ollama_config.stream = stream;

    if (skip_embedding == false) {
        std::vector<std::string> urls = {
            "https://raw.githubusercontent.com/rfprod/nx-ng-starter/refs/heads/main/README.md"};
        embedding_utils.create_embedings(urls);
    }

    if (rag == false) {
        ollama_client.query_model();
    } else {
        retrieving_utils.run();
    }

    if (interactive == true) {
        while (done == false) {
            done = ui.next_prompt(&prompt);
            if (done == true) {
                continue;
            }

            ollama_config.prompt = prompt;
            if (rag == false) {
                ollama_client.query_model();
            } else {
                retrieving_utils.run();
            }
            ui.request_redraw();
        }
        ui.end_session(EXIT_SUCCESS);
    } else {
        UserInterface::print_message_noninteractive("\n🏁 The session has ended.\n");
    }

    return 0;
}
