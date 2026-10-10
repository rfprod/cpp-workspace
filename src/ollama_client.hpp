#pragma once

#include "progress_spinner.hpp"
#include "user-interface.hpp"
#include <curl/curl.h>
#include <iostream>

/**
 * Configuration for sending a prompt to an Ollama model.
 *
 * @param model The name of the model to query (e.g., "llama2", "neural-chat").
 * @param prompt The text prompt to send to the model.
 * @param stream Whether to stream the response (true) or receive it all at once (false).
 */
struct OllamaPromptConfig {
    std::string model;
    std::string prompt;
    bool stream;
};

/**
 * URLs for Ollama API endpoints.
 *
 * @param generate_url The endpoint for querying a model.
 * @param tags_url The endpoint for listing available models.
 */
struct OllamaServerURLs {
    std::string generate_url;
    std::string tags_url;
};

/**
 * Client for communicating with a local Ollama instance.
 *
 * OllamaClient manages HTTP requests to Ollama's REST API, handling both
 * model queries and model listing. It integrates with a UserInterface for
 * displaying messages and errors, and uses ProgressSpinner to provide
 * visual feedback during network operations.
 *
 * The client verifies that the Ollama binary is installed on the system
 * and initializes CURL for HTTP communication. All responses are parsed
 * as JSON and formatted for display.
 *
 * @note Designed for local Ollama instances running on localhost:11434.
 *
 * @example Query a model:
 * @code
 * UserInterface ui;
 * OllamaClient client(&ui);
 *
 * OllamaPromptConfig config{
 *     .model = "llama2",
 *     .prompt = "What is machine learning?",
 *     .stream = false
 * };
 * client.query_model(config);
 * @endcode
 *
 * @example List available models:
 * @code
 * UserInterface ui;
 * OllamaClient client(&ui);
 * client.query_model_list();
 * @endcode
 */
class OllamaClient {
  private:
    /**
     * CURL handle for HTTP requests. Initialized in init_curl().
     */
    CURL* curl;

    /**
     * API endpoint URLs for the Ollama server.
     */
    OllamaServerURLs urls;

    /**
     * CURL headers list for HTTP requests (Content-Type, etc.).
     * Allocated in query methods and freed after each request.
     */
    struct curl_slist* headers;

    /**
     * Pointer to the UserInterface for displaying messages and errors.
     * Used for both CLI and interactive UI output.
     */
    UserInterface* ui;

    /**
     * Progress spinner for visual feedback during network operations.
     * Managed via std::unique_ptr for automatic cleanup.
     */
    std::unique_ptr<ProgressSpinner> spinner;

    /**
     * Verifies that the Ollama binary is available on the system.
     *
     * Uses the `which` command to check for Ollama in the PATH. If not found,
     * displays an error message and either exits (CLI mode) or ends the session
     * (interactive mode).
     *
     * Displays a checkmark on success.
     */
    void check_ollama();

    /**
     * Initializes the CURL library and allocates a CURL handle.
     *
     * If initialization fails, displays an error message and exits or ends
     * the session depending on the UI state.
     *
     * Displays a checkmark on success.
     */
    void init_curl();

    /**
     * Configures the CURL progress callback for download tracking.
     *
     * Sets up CURLOPT_XFERINFOFUNCTION and CURLOPT_XFERINFODATA to enable
     * progress tracking during HTTP transfers via the ProgressSpinner.
     *
     * @note Must be called before curl_easy_perform().
     */
    void setup_progress_callback();

    /**
     * Removes leading and trailing whitespace from a string.
     *
     * Uses std::find_if with lambda predicates to efficiently trim both ends
     * without creating intermediate strings.
     *
     * @param response The input string to trim.
     * @return The trimmed string.
     */
    std::string trim_response(std::string response);

    /**
     * Parses and displays a complete (non-streamed) JSON response from Ollama.
     *
     * Extracts the "response" field from the JSON and displays it formatted
     * with a llama emoji. If JSON parsing fails, displays an error and exits
     * or ends the session.
     *
     * @param response The complete JSON response from the Ollama API.
     */
    void process_response(std::string response);

    /**
     * Parses and displays a streamed JSON response from Ollama.
     *
     * Processes the response as newline-delimited JSON chunks, extracting
     * the "response" field from each chunk and printing it incrementally.
     * Stops processing when a chunk has "done" set to true.
     *
     * @param response The complete streamed response (newline-delimited JSON).
     */
    void process_response_stream(std::string response);

  public:
    /**
     * Constructs an OllamaClient connected to a UserInterface.
     *
     * Initializes the Ollama API endpoint URLs, verifies the Ollama binary
     * is installed, and creates a ProgressSpinner for network operations.
     *
     * @param ui Pointer to the UserInterface for displaying output. Must remain
     *           valid for the lifetime of this OllamaClient.
     *
     * @note Calls check_ollama() during construction. Exits if Ollama is not found.
     */
    explicit OllamaClient(UserInterface* ui);

    /**
     * Sends a prompt to an Ollama model and displays the response.
     *
     * Performs the following steps:
     * 1. Initializes CURL
     * 2. Starts the progress spinner
     * 3. Builds a JSON request with the model, prompt, and stream settings
     * 4. Sends a POST request to the generate endpoint
     * 5. Processes the response (streamed or complete)
     * 6. Cleans up CURL resources
     *
     * Displays both the user's prompt and the model's response via the UI.
     * Handles both CLI and interactive UI modes.
     *
     * @param config Configuration containing the model name, prompt, and stream setting.
     *
     * @note On error, displays an error message and exits or ends the session.
     */
    void query_model(OllamaPromptConfig config);

    /**
     * Retrieves and displays a list of available models from the Ollama server.
     *
     * Sends a GET request to the /api/tags endpoint and parses the response
     * to extract model metadata (name, size, modification date). Displays
     * each model with its information formatted as a table.
     *
     * @note On error, displays an error message and exits or ends the session.
     */
    void query_model_list();
};
