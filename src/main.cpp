#include "ollama_client.hpp"
#include <iostream>

int main(int argc, const char* argv[]) {
    std::string model_arg = "gemma3n:latest";
    std::string prompt_arg = "Hey! What's up!";
    bool stream_arg = false;

    for (int i = 1; i < argc; i += 1) {
        std::string arg = argv[i];
        if (arg.starts_with("--model=") == 0) {
            model_arg = arg.substr(8);
        } else if (arg.starts_with("--prompt=") == 0) {
            prompt_arg = arg.substr(9);
        } else if (arg.starts_with("--stream=") == 0) {
            stream_arg = arg.substr(9) == "true" ? true : false;
        }
    }

    printf("\n");
    printf("model_arg = %s\n", model_arg.c_str());
    printf("prompt_arg = %s\n", prompt_arg.c_str());
    printf("stream_arg = %s\n", stream_arg ? "true" : "false");
    printf("\n");

    if (prompt_arg.length() == 0) {
        printf("Enter your prompt: ");
        getline(std::cin, prompt_arg);
    }

    OllamaClient client = OllamaClient();

    struct OllamaPromptConfig config;
    config.model = model_arg;
    config.prompt = prompt_arg;
    config.stream = stream_arg;

    client.curl_request(config);

    return 0;
}
