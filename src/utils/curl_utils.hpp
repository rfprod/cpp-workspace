#pragma once

#include "http_utils.hpp"
#include "interface/progress_spinner.hpp"
#include "interface/user_interface.hpp"
#include <curl/curl.h>
#include <iostream>
#include <map>
#include <optional>
#include <string>

/**
 * Web content fetch result.
 */
struct WebContent {
    std::string read_buffer;
    char error_buffer[CURL_ERROR_SIZE];
    long status = 0;
};

/**
 * Web content stream result.
 */
struct WebContentStream {
    bool success;
    char error_buffer[CURL_ERROR_SIZE];
    std::string error_message;
    int status = 0;
};

/**
 * CURL HTTP client wrapper for making web requests to the Ollama server.
 *
 * Provides a simplified interface for HTTP operations by encapsulating CURL
 * initialization, request execution, and resource cleanup. Manages CURL easy
 * handles and HTTP headers, with support for both GET and POST requests via
 * optional POST data parameter. Integrates with the UserInterface for logging
 * and progress feedback during network operations. Follows RAII pattern with
 * automatic resource cleanup in the destructor. Uses a progress spinner to
 * provide visual feedback during potentially long-running HTTP operations.
 */
class CurlUtils {
  private:
    /**
     * CURL handle for HTTP requests. Initialized in init_curl().
     */
    CURL* curl;

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

    void initialize(void);

    void cleanup(void);

    long check_service_status(const std::string& url, const std::map<std::string, std::string>& extra_headers = {},
                              long timeout_seconds = 5L);

    static size_t curl_write_callback(char* contents, size_t size, size_t nmemb, std::string* userp);

    // Streaming methods.
    std::string stream_buffer;
    static size_t curl_streaming_write_callback(char* contents, size_t size, size_t nmemb, void* userp);
    void process_chunk(const std::string& chunk);
    std::function<void(const std::string&)> stream_callback;

  public:
    explicit CurlUtils(UserInterface* ui);

    ~CurlUtils();

    long wait_for_service(const std::string& url, const std::map<std::string, std::string>& extra_headers = {},
                          int max_retries = 3, int initial_delay_ms = 500);

    // Non-streaming version (buffers entire response)
    std::string fetch_web_content(const std::string& url, std::optional<std::string> post_data = std::nullopt,
                                  std::optional<std::string> custom_request_type = std::nullopt,
                                  std::optional<std::map<std::string, std::string>> extra_headers = std::nullopt,
                                  bool fail_on_http_error = true);

    // Streaming version (processes chunks as they arrive)
    WebContentStream fetch_web_content_stream(
        const std::string& url, std::optional<std::string> post_data, std::function<void(const std::string&)> on_chunk,
        std::optional<std::string> custom_request_type = std::nullopt,
        std::optional<std::map<std::string, std::string>> extra_headers = std::nullopt, bool fail_on_http_error = true);
};
