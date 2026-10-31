#pragma once

#include "interface/progress_spinner.hpp"
#include "interface/user_interface.hpp"
#include "utils/curl_utils.hpp"
#include "utils/system_utils.hpp"
#include <curl/curl.h>
#include <iostream>
#include <optional>

/**
 * Configuration for sending a prompt to an Ollama model.
 *
 * @param model The name of the model to query (e.g., "llama2", "neural-chat").
 * @param embedding_model The name of the embedding model to query (e.g., "embeddinggemma").
 * @param prompt The text prompt to send to the model.
 * @param stream Whether to stream the response (true) or receive it all at once (false).
 */
struct OllamaClientConfig {
    std::string model;
    std::string embedding_model;
    std::string prompt;
    bool stream;
};

/**
 * URLs for Ollama API endpoints.
 *
 * @param generate_url The endpoint for querying a model.
 * @param tags_url The endpoint for listing available models.
 * @param embed_url The endpoint for creating embeddings.
 */
struct OllamaServerURLs {
    std::string generate_url;
    std::string tags_url;
    std::string embed_url;
};

/**
 * Client for interacting with the Ollama API server.
 *
 * Manages communication with an Ollama server instance, providing functionality
 * to query language models, retrieve available models, and generate embeddings.
 * Encapsulates HTTP communication via CURL, response parsing, and dual-mode
 * output (console or interactive UI) based on the running state of the user
 * interface. Validates server connectivity on initialization and handles errors
 * with appropriate logging to either stderr or the UI.
 *
 * The client uses pre-configured API endpoint URLs and delegates HTTP operations
 * to a CURL wrapper utility. Responses from the server are processed and
 * displayed to the user through the provided UI interface.
 */
class OllamaClient {
  private:
    /**
     * API endpoint URLs for the Ollama server.
     */
    OllamaServerURLs urls;

    /**
     * Pointer to the Ollama Client configuraion.
     */
    OllamaClientConfig* config;

    /**
     * Pointer to the UserInterface for displaying messages and errors.
     */
    UserInterface* ui;

    /**
     * Pointer to the CURL wrapper containing pre-configured internal logic.
     */
    CurlUtils* curl;

    /**
     * Pointer to the SystemUtils for processing `system()` calls.
     */
    SystemUtils* system_utils;

    void check_ollama();

    std::string trim_response(std::string response);

  public:
    explicit OllamaClient(OllamaClientConfig* config, UserInterface* ui, CurlUtils* curl, SystemUtils* system_utils);

    ~OllamaClient();

    template <typename T> const T& get_config_value(const std::string& property_name) const;

    const std::string& get_url_value(const std::string& property_name) const;

    std::vector<std::vector<float>> generate_embeddings(const std::vector<std::string>& texts,
                                                        std::optional<OllamaClientConfig> config_opt = std::nullopt);

    void query_model(std::optional<OllamaClientConfig> config_opt = std::nullopt);

    void process_response(std::string response);

    void process_response_stream(const std::string& chunk);

    std::vector<std::string> query_model_list();
};
