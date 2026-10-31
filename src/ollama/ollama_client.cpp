#include "ollama_client.hpp"
#include <algorithm>
#include <cstdlib>
#include <curl/curl.h>
#include <format>
#include <iostream>
#include <nlohmann/json.hpp>
#include <sstream>
#include <string>

using nlohmann::json_abi_v3_11_2::json;

/**
 * Constructs an OllamaClient connected to a UserInterface.
 *
 * Initializes the Ollama API endpoint URLs, verifies the Ollama binary
 * is installed, and creates a ProgressSpinner for network operations.
 *
 * @param config Pointer to the Ollama Client configuration options.
 * @param ui Pointer to the UserInterface for displaying output.
 * @param curl Pointer to a CurlUtils instance for HTTP communication with Qdrant.
 * @param system_utils Pointer to a SustemUtils instance for `system()` call status handling.
 *
 * @note Calls check_ollama() during construction. Exits if Ollama is not found.
 */
OllamaClient::OllamaClient(OllamaClientConfig* config, UserInterface* ui, CurlUtils* curl, SystemUtils* system_utils) {
    this->urls = {
        generate_url : "http://localhost:11434/api/generate",
        tags_url : "http://localhost:11434/api/tags",
        embed_url : "http://localhost:11434/api/embed"
    };

    this->config = config;
    this->curl = curl;
    this->ui = ui;
    this->system_utils = system_utils;

    this->check_ollama();

    if (ui->is_running() == false) {
        UserInterface::print_message_noninteractive("\n");
    }
}

OllamaClient::~OllamaClient() {
    int status = system("echo \"\" > ~/.ollama/history");
    this->system_utils->catch_error(status);
}

template <typename T> const T& OllamaClient::get_config_value(const std::string& property_name) const {
    if (property_name == "model") {
        if constexpr (std::is_same_v<T, std::string>) {
            return this->config->model;
        }
    }

    if (property_name == "embedding_model") {
        if constexpr (std::is_same_v<T, std::string>) {
            return this->config->embedding_model;
        }
    }

    if (property_name == "prompt") {
        if constexpr (std::is_same_v<T, std::string>) {
            return this->config->prompt;
        }
    }

    if (property_name == "stream") {
        if constexpr (std::is_same_v<T, bool>) {
            return this->config->stream;
        }
    }

    throw std::runtime_error("Property not supported: " + property_name);
}
template const std::string& OllamaClient::get_config_value<std::string>(const std::string&) const;
template const bool& OllamaClient::get_config_value<bool>(const std::string&) const;

const std::string& OllamaClient::get_url_value(const std::string& property_name) const {
    if (property_name == "generate_url") {
        return this->urls.generate_url;
    }

    if (property_name == "embed_url") {
        return this->urls.embed_url;
    }

    throw std::runtime_error("Property not supported: " + property_name);
}

/**
 * Verifies that the Ollama binary is available on the system.
 *
 * Uses the `which ollama` command to check if Ollama is in the PATH.
 * - If found: displays a checkmark and continues.
 * - If not found: displays an error message with installation instructions
 *   and exits or ends the session.
 *
 * Output is routed through the UserInterface (if available) or stdout/stderr.
 *
 * Displays a checkmark on success.
 */
void OllamaClient::check_ollama() {
    std::string message{};
    if (system("which ollama > /dev/null 2>&1")) {
        message = "Ollama binary does not exist.\nInstallation instructions: "
                  "https://docs.ollama.com/linux\n";
        if (this->ui->is_running() == false) {
            UserInterface::print_error_noninteractive(message);
            exit(0);
        } else {
            this->ui->print_error(message);
            this->ui->end_session(EXIT_FAILURE);
        }
    } else {
        message = "Ollama binary exists.\n";
        if (this->ui->is_running() == false) {
            UserInterface::print_success_noninteractive(message);
        } else {
            this->ui->print_success(message);
        }
    }
};

/**
 * Removes leading and trailing whitespace from a string.
 *
 * Uses std::find_if with lambda predicates to efficiently trim both ends
 * without creating unnecessary intermediate strings.
 *
 * @param response The input string.
 * @return The trimmed string.
 */
std::string OllamaClient::trim_response(std::string response) {
    response.erase(response.begin(),
                   std::find_if(response.begin(), response.end(), [](unsigned char ch) { return !std::isspace(ch); }));

    response.erase(
        std::find_if(response.rbegin(), response.rend(), [](unsigned char ch) { return !std::isspace(ch); }).base(),
        response.end());
    return response;
}

/**
 * Parses and displays a complete JSON response from Ollama.
 *
 * Extracts the "response" field, trims whitespace, and displays it with
 * a llama emoji header and separator. On JSON parse error, displays an
 * error message and exits or ends the session.
 *
 * Output is routed through the UserInterface or stdout/stderr.
 *
 * @param response The complete JSON response from the Ollama API.
 */
void OllamaClient::process_response(std::string response) {
    try {
        json response_json = json::parse(response);

        if (this->ui->is_running() == false) {
            std::string message = std ::format(
                "🦙 llama:\n{}\n---\n", this->trim_response(response_json["response"].get<std::string>()).c_str());
            UserInterface::print_message_noninteractive(message);
        } else {
            this->ui->print_message("🦙 llama:");
            this->ui->print_message(this->trim_response(response_json["response"].get<std::string>()));
            this->ui->print_message("---");
        }
    } catch (const json::exception& err) {
        std::string message = std::format("JSON parse error: {}\n", err.what());
        if (this->ui->is_running() == false) {
            UserInterface::print_error_noninteractive(message);
            exit(1);
        } else {
            this->ui->print_error(message);
            this->ui->end_session(EXIT_FAILURE);
        }
    }
}

void OllamaClient::process_response_stream(const std::string& line) {
#ifdef DEBUG
    if (this->ui->is_running() == false) {
        UserInterface::print_message_noninteractive(
            std::format("\t[DEBUG] process_response_stream called with line: {}\n", << line.substr(0, 100)));
    }
#endif

    try {
        json chunk_json = json::parse(line);
        std::string text = chunk_json["response"].get<std::string>();

        // First chunk: stream=false (new entry), subsequent chunks: stream=true (append)
        static bool first_chunk = true;
        bool is_streaming = !first_chunk;
        first_chunk = false;

        if (this->ui->is_running() == false) {
            UserInterface::print_message_noninteractive(text);
        } else {
            this->ui->print_message(text, is_streaming);
        }

        if (chunk_json["done"].get<bool>()) {
            first_chunk = true; // Reset for next response
            return;
        }
    } catch (const json::exception& err) {
        std::string message = std::format("JSON parse error in process_response_stream: {}\nError ID: {}\nLine was: {}",
                                          err.what(), err.id, line);
        UserInterface::print_error_noninteractive(message);
    }
}

/**
 * Generates embeddings for a list of text strings using the Ollama embedding model.
 *
 * This method sends each text to the Ollama embedding endpoint and retrieves
 * the corresponding embedding vectors. Embeddings are processed sequentially.
 *
 * @param texts A vector of text strings to embed
 * @param config_opt Configuration object specifying the embedding model to use
 *
 * @return A vector of embedding vectors, where each inner vector contains
 *         the float values representing the embedding for the corresponding text.
 *         The order of embeddings matches the order of input texts.
 *
 * @throws std::exception if the web request fails or the response cannot be parsed
 */
std::vector<std::vector<float>> OllamaClient::generate_embeddings(const std::vector<std::string>& texts,
                                                                  std::optional<OllamaClientConfig> config_opt) {
    std::vector<std::vector<float>> embeddings{};

    json request_payload = {
        {"model", config_opt != std::nullopt ? config_opt->embedding_model : config->embedding_model}, {"input", ""}};

    for (const auto& text : texts) {
        request_payload["input"] = text;

        std::string post_data = request_payload.dump();

        std::string response = this->curl->fetch_web_content(this->urls.embed_url, post_data);

#ifdef DEBUG
        if (this->ui->is_running() == false) {
            UserInterface::print_message_noninteractive(
                std::format("\t[DEBUG] generate_embeddings response {}\n", response));
        }
#endif

        const json data = json::parse(response);

        embeddings.push_back(data["embeddings"][0].get<std::vector<float>>());
    }

    return embeddings;
}

/**
 * Sends a prompt query to the Ollama model and processes the response.
 *
 * Displays the formatted user message to either the console or UI based on
 * the UI's running state, then constructs a JSON request payload with the
 * model, prompt, and stream settings. Fetches the response from the Ollama
 * server and processes it either as a stream or as a complete response.
 *
 * @param config_opt The prompt configuration containing the model name, prompt
 *               text, and stream preference
 */
void OllamaClient::query_model(std::optional<OllamaClientConfig> config_opt) {
    std::string message =
        std::format("💬 user: {}\n", config_opt != std::nullopt ? config_opt->prompt.c_str() : config->prompt.c_str());
    if (this->ui->is_running() == false) {
        UserInterface::print_message_noninteractive(message);
    } else {
        this->ui->print_message(message);
    }

    json request_payload;
    if (config_opt != std::nullopt) {
        request_payload = {
            {"model", config_opt->model}, {"prompt", config_opt->prompt}, {"stream", config_opt->stream}};
    } else {
        request_payload = {{"model", config->model}, {"prompt", config->prompt}, {"stream", config->stream}};
    }

    std::string post_data = request_payload.dump();

    bool stream = config_opt != std::nullopt ? config_opt->stream : config->stream;
    if (stream) {
        WebContentStream response =
            this->curl->fetch_web_content_stream(this->urls.generate_url, post_data, [this](const std::string& chunk) {
                this->process_response_stream(chunk);
            });
        message = std::format("Stream successful: {}, Status: {}\n", response.success, response.status);
        if (this->ui->is_running() == false) {
            UserInterface::print_message_noninteractive(message);
        } else {
            this->ui->print_message(message);
        }
    } else {
        std::string response = this->curl->fetch_web_content(this->urls.generate_url, post_data);
        this->process_response(response);
    }
}

/**
 * Retrieves a list of available models from the Ollama server.
 *
 * Fetches the models list from the Ollama tags endpoint and parses the JSON
 * response. Validates that the response contains a valid models array. Extracts
 * and displays model metadata including name, modification timestamp, and size
 * (converted from bytes to megabytes). Collects valid model names into a vector
 * for return. Handles errors (invalid response structure or JSON parsing failures)
 * by terminating with an error message directed to either stderr or the UI based
 * on the UI's running state.
 *
 * @return A vector of available model names extracted from the server response.
 *         Returns an empty vector if an error occurs during fetch or parsing.
 */
std::vector<std::string> OllamaClient::query_model_list() {
    std::string message{};
    std::vector<std::string> models;

    std::string response = this->curl->fetch_web_content(this->urls.tags_url);

    try {
        const json data = json::parse(response);

        if (!data.contains("models") || !data["models"].is_array()) {
            message = "Response has no models array.\n";
            if (this->ui->is_running() == false) {
                UserInterface::print_error_noninteractive(message);
                exit(1);
            } else {
                this->ui->print_error(message);
                this->ui->end_session(EXIT_FAILURE);
            }
        }

        message = "📃 available models:\n";
        if (this->ui->is_running() == false) {
            UserInterface::print_message_noninteractive(message);
        } else {
            this->ui->print_message(message);
        }

        for (const auto& model : data["models"]) {
            const std::string name = model.value("name", "unknown");
            const std::string modified_at = model.value("modified_at", "unknown");
            const std::string size = model.contains("size")
                                         ? std::to_string(model["size"].get<long long>() / 1024 / 1024) + " Mb"
                                         : "size unknown";

            if (name != "unknown") {
                models.push_back(name);
            }

            message = std::format("{}\t ({}) \t{}\n", modified_at, size, name);
            if (this->ui->is_running() == false) {
                UserInterface::print_message_noninteractive(message);
            } else {
                this->ui->print_message(message);
            }
        }
        UserInterface::print_message_noninteractive("\n");
    } catch (const json::exception& err) {
        message = std::format("JSON parse error: {}\n", err.what());
        if (this->ui->is_running() == false) {
            UserInterface::print_error_noninteractive(message);
            exit(1);
        } else {
            this->ui->print_error(message);
            this->ui->end_session(EXIT_FAILURE);
        }
    }

    return models;
}
