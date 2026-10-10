#include "ollama_client.hpp"
#include <algorithm>
#include <curl/curl.h>
#include <format>
#include <iostream>
#include <nlohmann/json.hpp>
#include <sstream>
#include <string>

using nlohmann::json_abi_v3_11_2::json;

/**
 * CURL write callback for accumulating response data.
 *
 * Called by CURL whenever data is received. Appends the chunk to the
 * userp string buffer.
 *
 * @param contents Pointer to the received data chunk.
 * @param size Size of each element (always 1 for this use case).
 * @param nmemb Number of elements received.
 * @param userp Pointer to the std::string buffer to append data to.
 *
 * @return Total bytes processed (size * nmemb).
 */
static size_t WriteCallback(char* contents, size_t size, size_t nmemb, std::string* userp) {
    userp->append(contents, size * nmemb);
    return size * nmemb;
}

/**
 * Constructs an OllamaClient and initializes Ollama API endpoints.
 *
 * Sets up endpoint URLs for the local Ollama instance (localhost:11434),
 * verifies that Ollama is installed, and creates a ProgressSpinner for
 * use during network operations.
 *
 * Prints a newline in CLI mode for clean output formatting.
 */
OllamaClient::OllamaClient(UserInterface* ui) {
    this->urls = {
        generate_url : "http://localhost:11434/api/generate",
        tags_url : "http://localhost:11434/api/tags"
    };
    this->curl = nullptr;
    this->headers = nullptr;
    this->ui = ui;
    this->spinner = std::make_unique<ProgressSpinner>(ui, false);

    this->check_ollama();

    if (ui->is_running() == false) {
        printf("\n");
    }
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
 */
void OllamaClient::check_ollama() {
    if (system("which ollama > /dev/null 2>&1")) {
        std::string message = "Ollama binary does not exist.\nInstallation instructions: "
                              "https://docs.ollama.com/linux\n";
        if (this->ui->is_running() == false) {
            fprintf(stderr, "❌ %s", message.c_str());
            exit(0);
        } else {
            this->ui->print_error(message);
            this->ui->end_session(EXIT_FAILURE);
        }
    } else {
        std::string message = "Ollama binary exists.\n";
        if (this->ui->is_running() == false) {
            printf("✅ %s", message.c_str());
        } else {
            this->ui->print_success(message);
        }
    }
};

/**
 * Initializes the CURL library and allocates a CURL handle.
 *
 * On failure, displays an error message and either exits (CLI mode) or
 * ends the session (interactive mode). On success, displays a checkmark.
 */
void OllamaClient::init_curl() {
    this->curl = curl_easy_init();
    if (!this->curl) {
        std::string message = "Failed to initialize CURL.\n";
        if (this->ui->is_running() == false) {
            fprintf(stderr, "❌ %s", message.c_str());
            exit(1);
        } else {
            this->ui->print_error(message);
            this->ui->end_session(EXIT_FAILURE);
        }
    } else {
        std::string message = "CURL initialized.\n";
        if (this->ui->is_running() == false) {
            printf("✅ %s", message.c_str());
        } else {
            this->ui->print_success(message);
        }
    }
}

/**
 * Configures CURL's progress callback for download tracking.
 *
 * Enables CURLOPT_NOPROGRESS and sets the progress callback function and
 * data pointer. This allows the ProgressSpinner to update during network
 * transfers.
 *
 * Does nothing if curl is nullptr.
 */
void OllamaClient::setup_progress_callback() {
    if (this->curl == nullptr) {
        return;
    }

    curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 0L);
    curl_easy_setopt(curl, CURLOPT_XFERINFOFUNCTION, ProgressSpinner::progress_callback);
    curl_easy_setopt(curl, CURLOPT_XFERINFODATA, spinner.get());
}

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
                   std::find_if(response.begin(), response.end(),
                                [](unsigned char ch) { return !std::isspace(ch); }));

    response.erase(std::find_if(response.rbegin(), response.rend(),
                                [](unsigned char ch) { return !std::isspace(ch); })
                       .base(),
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
            printf("🦙 llama:\n%s\n---\n",
                   this->trim_response(response_json["response"].get<std::string>()).c_str());
        } else {
            this->ui->print_message("🦙 llama:");
            this->ui->print_message(
                this->trim_response(response_json["response"].get<std::string>()));
            this->ui->print_message("---");
        }
    } catch (const json::exception& err) {
        std::string message = std::format("JSON parse error: {}\n", err.what());
        if (this->ui->is_running() == false) {
            fprintf(stderr, "❌ %s", message.c_str());
            exit(1);
        } else {
            this->ui->print_error(message);
            this->ui->end_session(EXIT_FAILURE);
        }
    }
}

/**
 * Parses and displays a streamed JSON response from Ollama.
 *
 * Processes the response as newline-delimited JSON chunks. For each chunk:
 * 1. Extracts the "response" field (text fragment)
 * 2. Prints it incrementally to stdout
 * 3. Checks the "done" field to determine if streaming is complete
 *
 * On JSON parse error, displays an error message and exits or ends the session.
 *
 * @param response The complete streamed response (newline-delimited JSON).
 */
void OllamaClient::process_response_stream(std::string response) {
    std::istringstream stream(response);
    std::string line;

    std::cout << "Response: ";
    std::cout.flush();

    while (std::getline(stream, line)) {
        if (!line.empty()) {
            try {
                json chunk = json::parse(line);
                std::string text = chunk["response"].get<std::string>();
                std::cout << text;
                std::cout.flush();

                if (chunk["done"].get<bool>()) {
                    break;
                }
            } catch (const json::exception& err) {
                std::string message = std::format("JSON parse error: {}\n", err.what());
                if (this->ui->is_running() == false) {
                    fprintf(stderr, "❌ %s", message.c_str());
                    exit(1);
                } else {
                    this->ui->print_error(message);
                    this->ui->end_session(EXIT_FAILURE);
                }
            }
        }
    }

    std::cout << std::endl;
}

/**
 * Sends a prompt to an Ollama model and displays the response.
 *
 * Executes the following workflow:
 * 1. Initializes CURL and starts the progress spinner
 * 2. Formats and displays the user's prompt via the UI
 * 3. Constructs a JSON request payload with model, prompt, and stream settings
 * 4. Sends a POST request to the Ollama generate endpoint
 * 5. Validates HTTP response status (expects 200 OK)
 * 6. Processes the response as either streamed or complete JSON
 * 7. Cleans up CURL resources (handle and headers list)
 *
 * The prompt is displayed immediately with a "💬 user:" prefix. The model's
 * response is processed based on the stream setting:
 * - If stream=true: Response chunks are printed incrementally as they arrive
 * - If stream=false: Entire response is printed after completion
 *
 * @param config OllamaPromptConfig containing:
 *               - model: Name of the model to query (e.g., "llama2")
 *               - prompt: The text prompt to send
 *               - stream: Whether to stream the response
 *
 * @note Error handling depends on UI mode:
 *       - CLI mode: Prints error to stderr and calls exit(1)
 *       - Interactive mode: Calls ui->print_error() and ui->end_session(EXIT_FAILURE)
 *
 * @note CURL errors include network failures, connection timeouts, and invalid URLs.
 *       HTTP errors are reported when Ollama returns non-200 status codes.
 *
 * @warning The method will not return if an error occurs; it terminates the program
 *          or session instead.
 */
void OllamaClient::query_model(OllamaPromptConfig config) {
    this->init_curl();

    this->spinner->start();

    std::string message = std::format("💬 user: {}\n", config.prompt.c_str());
    if (this->ui->is_running() == false) {
        printf("%s", message.c_str());
    } else {
        this->ui->print_message(message);
    }

    json request_payload = {
        {"model", config.model}, {"prompt", config.prompt}, {"stream", config.stream}};

    std::string post_data = request_payload.dump();

    std::string response;

    char error_buffer[CURL_ERROR_SIZE];

    curl_easy_setopt(this->curl, CURLOPT_URL, this->urls.generate_url.c_str());
    curl_easy_setopt(this->curl, CURLOPT_POSTFIELDS, post_data.c_str());
    curl_easy_setopt(this->curl, CURLOPT_WRITEFUNCTION, WriteCallback);
    curl_easy_setopt(this->curl, CURLOPT_WRITEDATA, &response);
    curl_easy_setopt(this->curl, CURLOPT_ERRORBUFFER, error_buffer);

    this->setup_progress_callback();

    this->headers = curl_slist_append(this->headers, "Content-Type: application/json");
    curl_easy_setopt(this->curl, CURLOPT_HTTPHEADER, this->headers);

    CURLcode result = curl_easy_perform(this->curl);
    this->spinner->stop();

    long status = 0;
    curl_easy_getinfo(this->curl, CURLINFO_RESPONSE_CODE, &status);

    curl_easy_cleanup(this->curl);
    curl_slist_free_all(this->headers);
    this->headers = nullptr;

    if (result != CURLE_OK) {
        message = std::format("CURL error: {}\n", curl_easy_strerror(result));
        if (this->ui->is_running() == false) {
            fprintf(stderr, "%s", message.c_str());
            if (error_buffer[0] != '\0') {
                message = std::format("Details: {}\n", error_buffer);
                fprintf(stderr, "%s", message.c_str());
            }
            exit(1);
        } else {
            this->ui->print_error(message);
            if (error_buffer[0] != '\0') {
                message = std::format("Details: {}\n", error_buffer);
                this->ui->print_error(message);
            }
            this->ui->end_session(EXIT_FAILURE);
        }
    }
    if (status != 200) {
        message = std::format("Ollama returned HTTP status: {}\n", status);
        if (this->ui->is_running() == false) {
            fprintf(stderr, "%s", message.c_str());
            exit(1);
        } else {
            this->ui->print_error(message);
            this->ui->end_session(EXIT_FAILURE);
        }
    }

    if (config.stream) {
        this->process_response_stream(response);
    } else {
        this->process_response(response);
    }
}

/**
 * Retrieves and displays all available models from the Ollama server.
 *
 * Executes the following workflow:
 * 1. Initializes CURL and starts the progress spinner
 * 2. Sends a GET request to the Ollama /api/tags endpoint
 * 3. Validates HTTP response status (expects 200 OK)
 * 4. Parses the JSON response to extract the "models" array
 * 5. Displays each model with metadata in a formatted table
 * 6. Cleans up CURL resources (handle and headers list)
 *
 * For each model, displays:
 * - Last modified timestamp
 * - Size in megabytes (or "size unknown" if unavailable)
 * - Model name
 *
 * Output format: "{modified_at}\t ({size_mb}) \t{name}"
 *
 * @note Error handling depends on UI mode:
 *       - CLI mode: Prints error to stderr and calls exit(1)
 *       - Interactive mode: Calls ui->print_error() and ui->end_session(EXIT_FAILURE)
 *
 * @note Errors may occur at multiple stages:
 *       - CURL errors: Network failures, connection issues
 *       - HTTP errors: Non-200 response codes from Ollama
 *       - JSON errors: Malformed response or missing "models" field
 *
 * @warning The method will not return if an error occurs; it terminates the program
 *          or session instead.
 *
 * @see OllamaPromptConfig for model information structure
 */
void OllamaClient::query_model_list() {
    this->init_curl();

    this->spinner->start();

    std::string response;

    char error_buffer[CURL_ERROR_SIZE];

    curl_easy_setopt(this->curl, CURLOPT_URL, this->urls.tags_url.c_str());
    curl_easy_setopt(this->curl, CURLOPT_WRITEFUNCTION, WriteCallback);
    curl_easy_setopt(this->curl, CURLOPT_WRITEDATA, &response);
    curl_easy_setopt(this->curl, CURLOPT_ERRORBUFFER, error_buffer);

    this->setup_progress_callback();

    this->headers = curl_slist_append(this->headers, "Content-Type: application/json");
    curl_easy_setopt(this->curl, CURLOPT_HTTPHEADER, this->headers);

    CURLcode result = curl_easy_perform(this->curl);
    this->spinner->stop();

    long status = 0;
    curl_easy_getinfo(this->curl, CURLINFO_RESPONSE_CODE, &status);

    curl_easy_cleanup(this->curl);
    curl_slist_free_all(this->headers);
    this->headers = nullptr;

    std::string message{};
    if (result != CURLE_OK) {
        message = std::format("CURL error: {}\n", curl_easy_strerror(result));
        if (this->ui->is_running() == false) {
            fprintf(stderr, "❌ %s", message.c_str());
            if (error_buffer[0] != '\0') {
                message = std::format("Details: {}\n", error_buffer);
                fprintf(stderr, "%s", message.c_str());
            }
            exit(1);
        } else {
            this->ui->print_error(message);
            if (error_buffer[0] != '\0') {
                message = std::format("Details: {}\n", error_buffer);
                this->ui->print_error(message);
            }
            this->ui->end_session(EXIT_FAILURE);
        }
    }
    if (status != 200) {
        message = std::format("Ollama returned HTTP status: {}\n", status);
        if (this->ui->is_running() == false) {
            fprintf(stderr, "❌ %s", message.c_str());
            exit(1);
        } else {
            this->ui->print_error(message);
            this->ui->end_session(EXIT_FAILURE);
        }
    }

    try {
        const json data = json::parse(response);

        if (!data.contains("models") || !data["models"].is_array()) {
            message = "Response has no models array.\n";
            if (this->ui->is_running() == false) {
                fprintf(stderr, "❌ %s", message.c_str());
                exit(1);
            } else {
                this->ui->print_error(message);
                this->ui->end_session(EXIT_FAILURE);
            }
        }

        message = "📃 available models:\n";
        if (this->ui->is_running() == false) {
            printf("%s", message.c_str());
        } else {
            this->ui->print_message(message);
        }

        for (const auto& model : data["models"]) {
            const std::string name = model.value("name", "unknown");
            const std::string modified_at = model.value("modified_at", "unknown");
            const std::string size =
                model.contains("size")
                    ? std::to_string(model["size"].get<long long>() / 1024 / 1024) + " Mb"
                    : "size unknown";

            message = std::format("{}\t ({}) \t{}\n", modified_at, size, name);
            if (this->ui->is_running() == false) {
                printf("%s", message.c_str());
            } else {
                this->ui->print_message(message);
            }
        }
        printf("\n");
    } catch (const json::exception& err) {
        message = std::format("JSON parse error: {}\n", err.what());
        if (this->ui->is_running() == false) {
            fprintf(stderr, "❌ %s", message.c_str());
            exit(1);
        } else {
            this->ui->print_error(message);
            this->ui->end_session(EXIT_FAILURE);
        }
    }
}
