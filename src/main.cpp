#include "ollama_client.hpp"
#include <iostream>

int main(int argc, const char* argv[]) {
    std::string model = "gemma3n:latest";
    std::string prompt = "Hey! What's up!";
    bool stream = false;

    for (int i = 1; i < argc; i += 1) {
        std::string arg = argv[i];
        if (arg.starts_with("--model=") == 0) {
            model = arg.substr(8);
        } else if (arg.starts_with("--prompt=") == 0) {
            prompt = arg.substr(9);
        } else if (arg.starts_with("--stream=") == 0) {
            stream = arg.substr(9) == "true" ? true : false;
        }
    }

    printf("\n");
    printf("🛈 model = %s\n", model.c_str());
    printf("🛈 prompt = %s\n", prompt.c_str());
    printf("🛈 stream = %s\n", stream ? "true" : "false");
    printf("\n");

    if (prompt.length() == 0) {
        printf("Enter your prompt: ");
        getline(std::cin, prompt);
    }

    OllamaClient client = OllamaClient();

    client.query_model_list();

    struct OllamaPromptConfig config;
    config.model = model;
    config.prompt = prompt;
    config.stream = stream;

    client.query_model(config);

    return 0;
}
