#include "interface/user_interface.hpp"
#include "ollama/ollama_client.hpp"
#include "storage/qdrant_client.hpp"
#include "utils/args_utils.hpp"
#include "utils/curl_utils.hpp"
#include "utils/embedding_utils.hpp"
#include "utils/exit_request.hpp"
#include "utils/retrieving_utils.hpp"
#include "utils/system_utils.hpp"
#include "utils/text_utils.hpp"
#include <iostream>
#include <unistd.h>
#include <vector>

int main(int argc, const char* argv[]) {

    ArgsUtils args_utils;
    struct ParsedArgs parsed_args = args_utils.parse_args(argc, argv);

    if (parsed_args.help == true) {
        args_utils.print_help();
    }

    try {
        bool done = false;
        std::string message{};

        auto ui = UserInterface::create(parsed_args.dark_theme);

        if (parsed_args.interactive == true) {
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

            ui->run();
            ui->print_args(parsed_args.model, parsed_args.embedding_model, parsed_args.prompt, parsed_args.stream,
                           parsed_args.rag, parsed_args.interactive, parsed_args.dark_theme,
                           parsed_args.skip_embedding);

            while (parsed_args.prompt.length() == 0) {
                done = ui->next_prompt(&parsed_args.prompt);

                if (done == true) {
                    ui->end_session(EXIT_SUCCESS);
                }
            }
        } else {
            UserInterface::print_args_noninteractive(parsed_args.model, parsed_args.embedding_model, parsed_args.prompt,
                                                     parsed_args.stream, parsed_args.rag, parsed_args.interactive,
                                                     parsed_args.dark_theme, parsed_args.skip_embedding);

            if (parsed_args.prompt.length() == 0) {
                done = UserInterface::next_prompt_noninteractive(&parsed_args.prompt);

                if (parsed_args.prompt.length() == 0) {
                    UserInterface::print_message_noninteractive("\n🏁 Empty prompt. The session has ended.\n");
                    throw ExitRequest{EXIT_FAILURE};
                }

                if (done == true) {
                    UserInterface::print_message_noninteractive("\n🏁 The session has ended.\n");
                    throw ExitRequest{EXIT_FAILURE};
                }
            }
        }

        auto curl = std::make_shared<CurlUtils>(ui);

        auto system_utils = std::make_shared<SystemUtils>(ui);

        auto storage_client = std::make_shared<QdrantClient>(ui, curl, system_utils);

        auto ollama_config = std::make_shared<OllamaClientConfig>();

        auto ollama_client = std::make_shared<OllamaClient>(ollama_config, ui, curl, system_utils);

        auto embedding_utils = std::make_shared<EmbeddingUtils>(ui, curl, ollama_client, storage_client);

        auto retrieving_config = std::make_shared<RetrievingUtilsConfig>();

        auto retrieving_utils =
            std::make_shared<RetrievingUtils>(retrieving_config, ui, curl, ollama_client, storage_client);

        std::vector<std::string> models = ollama_client->query_model_list();
        bool force_model_selection = true;
        for (size_t i = 0; i < models.size(); i += 1) {
            if (models[i] == parsed_args.model) {
                force_model_selection = false;
            }
        }

        if (force_model_selection == true) {
            message = "⚠ A Language model is not selected.\n";
            if (parsed_args.interactive == true) {
                ui->print_message(message);
                ui->print_model_list(models);
                ui->select_model(models, &parsed_args.model);
                ui->print_args(parsed_args.model, parsed_args.embedding_model, parsed_args.prompt, parsed_args.stream,
                               parsed_args.rag, parsed_args.interactive, parsed_args.dark_theme,
                               parsed_args.skip_embedding);
            } else {
                UserInterface::print_message_noninteractive(message);
                UserInterface::print_model_list_noninteractive(models);
                UserInterface::select_model_noninteractive(models, &parsed_args.model);
                UserInterface::print_args_noninteractive(
                    parsed_args.model, parsed_args.embedding_model, parsed_args.prompt, parsed_args.stream,
                    parsed_args.rag, parsed_args.interactive, parsed_args.dark_theme, parsed_args.skip_embedding);
            }
        }

        if (parsed_args.skip_embedding == false) {
            bool force_embedding_model_selection = true;
            for (size_t i = 0; i < models.size(); i += 1) {
                if (models[i] == parsed_args.embedding_model) {
                    force_embedding_model_selection = false;
                }
            }
            if (force_embedding_model_selection == true) {
                message = "⚠ An Embedding model is not selected.\n";
                if (parsed_args.interactive == true) {
                    ui->print_message(message);
                    ui->print_model_list(models);
                    ui->select_model(models, &parsed_args.embedding_model);
                    ui->print_args(parsed_args.model, parsed_args.embedding_model, parsed_args.prompt,
                                   parsed_args.stream, parsed_args.rag, parsed_args.interactive, parsed_args.dark_theme,
                                   parsed_args.skip_embedding);
                } else {
                    UserInterface::print_message_noninteractive(message);
                    UserInterface::print_model_list_noninteractive(models);
                    UserInterface::select_model_noninteractive(models, &parsed_args.embedding_model);
                    UserInterface::print_args_noninteractive(
                        parsed_args.model, parsed_args.embedding_model, parsed_args.prompt, parsed_args.stream,
                        parsed_args.rag, parsed_args.interactive, parsed_args.dark_theme, parsed_args.skip_embedding);
                }
            }
        }

        ollama_config->model = parsed_args.model;
        ollama_config->embedding_model = parsed_args.embedding_model;
        ollama_config->prompt = parsed_args.prompt;
        ollama_config->stream = parsed_args.stream;

        if (parsed_args.skip_embedding == false) {
            std::vector<std::string> urls = {
                "https://raw.githubusercontent.com/rfprod/nx-ng-starter/refs/heads/main/README.md"};
            embedding_utils->create_embedings(urls);
        }

        if (parsed_args.rag == false) {
            ollama_client->query_model();
        } else {
            retrieving_utils->run();
        }

        if (parsed_args.interactive == true) {
            while (done == false) {
                done = ui->next_prompt(&parsed_args.prompt);
                if (done == true) {
                    continue;
                }

                ollama_config->prompt = parsed_args.prompt;
                if (parsed_args.rag == false) {
                    ollama_client->query_model();
                } else {
                    retrieving_utils->run();
                }
                ui->request_redraw();
            }
            ui->end_session(EXIT_SUCCESS);
        } else {
            UserInterface::print_message_noninteractive("\n🏁 The session has ended.\n");
        }
    } catch (const ExitRequest& req) {
        UserInterface::print_message_noninteractive("\n🏁 The session has ended.\n");
        return req.code();
    }

    return EXIT_SUCCESS;
}
