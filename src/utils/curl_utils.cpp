#include "curl_utils.hpp"
#include <format>
#include <string>

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
 * Constructs a CurlUtils instance with dependency injection.
 *
 * Initializes CURL and headers pointers to nullptr, stores the provided
 * UserInterface pointer for logging and status messages, and creates a
 * ProgressSpinner instance for visual feedback during network operations.
 * The actual CURL easy handle initialization is deferred to the initialize()
 * method, typically called after object construction.
 *
 * @param ui Pointer to the UserInterface for displaying messages and progress
 */
CurlUtils::CurlUtils(UserInterface* ui) {
    this->curl = nullptr;
    this->headers = nullptr;
    this->ui = ui;
    this->spinner = std::make_unique<ProgressSpinner>(ui, false);
}

CurlUtils::~CurlUtils() {
    this->cleanup();
}

/**
 * Initializes the CURL easy handle for HTTP operations.
 *
 * Attempts to create a CURL easy handle via curl_easy_init(). If initialization
 * fails, logs an error message to either stderr or the UI based on the UI's
 * running state and terminates the program with EXIT_FAILURE. On successful
 * initialization, logs a confirmation message to either the console or UI.
 *
 * @throws Indirectly exits the program if CURL initialization fails.
 */
void CurlUtils::initialize() {
    this->curl = curl_easy_init();
    std::string message{};
    if (!this->curl) {
        message = "❌ Failed to initialize CURL.\n";
        if (this->ui->is_running() == false) {
            UserInterface::print_error_noninteractive(message);
            exit(1);
        } else {
            this->ui->print_error(message);
            this->ui->end_session(EXIT_FAILURE);
        }
    } else {
        message = "✅ CURL is initialized.\n";
        if (this->ui->is_running() == false) {
            UserInterface::print_message_noninteractive(message);
        } else {
            this->ui->print_success(message);
        }
    }
}

/**
 * Cleans up CURL resources and frees allocated memory.
 *
 * Destroys the CURL easy handle and frees the headers list. Sets the headers
 * pointer to nullptr to prevent use-after-free. Logs a success message to
 * either the console or UI based on the UI's running state. Typically called
 * during object destruction or when shutting down HTTP operations.
 *
 * @note Assumes cleanup operations succeed without error checking.
 */
void CurlUtils::cleanup() {
    curl_easy_cleanup(this->curl);
    curl_slist_free_all(this->headers);
    this->headers = nullptr;
    std::string message = "✅ CURL cleanup is complete.\n";
    if (this->ui->is_running() == false) {
        UserInterface::print_message_noninteractive(message);
    } else {
        this->ui->print_success(message);
    }
}

/**
 * @brief Checks service availability by querying an HTTP endpoint and returning the HTTP status code.
 *
 * Makes a silent HEAD request to the specified URL with optional custom headers,
 * discards the response body, and returns the HTTP status code.
 * On curl failure, returns -1.
 *
 * @param url Target endpoint URL (e.g., "http://localhost:6333/collections").
 * @param extra_headers Map of custom headers to include (e.g., {"api-key": "secret", "Authorization": "Bearer
 * token"}). If empty, request is sent without additional headers.
 * @param timeout_seconds Connection and transfer timeout in seconds (default: 5).
 * @return HTTP status code on success (e.g., 200, 401, 404); -1 on curl failure.
 * @note Initializes and cleans up curl handle internally; manages spinner lifecycle.
 * @see initialize(), cleanup()
 */
long CurlUtils::check_service_status(const std::string& url, const std::map<std::string, std::string>& extra_headers,
                                     long timeout_seconds) {
    this->initialize();
    this->spinner->start();

    long http_code = 0;

    curl_easy_setopt(this->curl, CURLOPT_URL, url.c_str());

    curl_easy_setopt(this->curl, CURLOPT_CUSTOMREQUEST, "GET");

    curl_easy_setopt(this->curl, CURLOPT_WRITEFUNCTION,
                     [](void*, size_t size, size_t nmemb, void*) { return size * nmemb; });
    curl_easy_setopt(this->curl, CURLOPT_TIMEOUT, timeout_seconds);
    curl_easy_setopt(this->curl, CURLOPT_CONNECTTIMEOUT, timeout_seconds);
    curl_easy_setopt(this->curl, CURLOPT_FOLLOWLOCATION, 1L);

    curl_easy_setopt(this->curl, CURLOPT_NOPROGRESS, 0L);
    curl_easy_setopt(this->curl, CURLOPT_XFERINFOFUNCTION, ProgressSpinner::progress_callback);
    curl_easy_setopt(this->curl, CURLOPT_XFERINFODATA, this->spinner.get());

    this->headers = curl_slist_append(this->headers, "Content-Type: application/json");

    for (const auto& [key, value] : extra_headers) {
        std::string header = key + ": " + value;
        this->headers = curl_slist_append(this->headers, header.c_str());
    }

    curl_easy_setopt(this->curl, CURLOPT_HTTPHEADER, this->headers);

    CURLcode res = curl_easy_perform(this->curl);
    this->spinner->stop();

    curl_easy_getinfo(this->curl, CURLINFO_RESPONSE_CODE, &http_code);

    this->cleanup();

    if (res != CURLE_OK) {
        std::string message = std::format("Curl error: {}\n", curl_easy_strerror(res));
        if (this->ui->is_running() == false) {
            UserInterface::print_error_noninteractive(message);
        } else {
            this->ui->print_error(message);
        }

        return -1;
    }
    return http_code;
}

/**
 * @brief Waits for a service to become available with retries and exponential backoff.
 *
 * @param url Target endpoint URL.
 * @param extra_headers Map of custom headers.
 * @param max_retries Number of retry attempts (default: 10).
 * @param initial_delay_ms Initial delay in milliseconds (default: 500ms).
 * @return HTTP status code on success; -1 if service unavailable after retries.
 */
long CurlUtils::wait_for_service(const std::string& url, const std::map<std::string, std::string>& extra_headers,
                                 int max_retries, int initial_delay_ms) {
    long http_code = -1;
    int delay_ms = initial_delay_ms;
    std::string message{};

    for (int attempt = 0; attempt < max_retries; attempt += 1) {
        http_code = this->check_service_status(url, extra_headers);

        if (http_code != -1) {
            message = std::format("✅ Service available (HTTP {}) after {} attempt(s)\n", http_code, attempt + 1);
            if (this->ui->is_running() == false) {
                UserInterface::print_message_noninteractive(message);
            } else {
                this->ui->print_success(message);
            }
            return http_code;
        }

        if (attempt < max_retries - 1) {
            message = std::format("⏳ Waiting for service... (attempt {}/{}, retrying in {}ms)\n", attempt + 1,
                                  max_retries, delay_ms);
            if (this->ui->is_running() == false) {
                UserInterface::print_message_noninteractive(message);
            } else {
                this->ui->print_message(message);
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(delay_ms));
            delay_ms = std::min(delay_ms * 2, 5000);
        }
    }

    message = std::format("Service unavailable after {} attempts\n", max_retries);
    if (this->ui->is_running() == false) {
        UserInterface::print_error_noninteractive(message);
    } else {
        this->ui->print_error(message);
    }
    return http_code;
}

/**
 * @brief Fetches web content from the specified URL via HTTP/HTTPS request.
 *
 * Performs an HTTP GET or POST request to the given URL using libcurl,
 * with automatic redirect following and JSON content-type header.
 * Shows a spinner while the request is in progress.
 *
 * @param url The target URL to fetch from. Must be a valid URL string.
 * @param post_data Optional POST request body. If provided, the request
 *                  will be a POST request; otherwise a GET request.
 *                  Defaults to std::nullopt (GET request).
 * @param custom_request_type Custom request type: GET, POST, PUT. Primarily used for PUT.
 * @param extra_headers Optional map of custom headers to include (e.g., {"Authorization": "Bearer token"}).
 *                      Content-Type application/json is always added automatically.
 *                      Defaults to std::nullopt (no extra headers).
 * @param fail_on_http_error If false, returns response body even on non-200 status codes.
 *                           If true (default), exits on non-200 status. Used for existence checks.
 *                           Defaults to true.
 *
 * @return A string containing the response body.
 *
 * @throws Calls exit(1) on CURL errors or non-200 HTTP status codes
 *         if the UI is not running. If UI is running, terminates the
 *         session with EXIT_FAILURE instead.
 *
 * @note Initializes and cleans up CURL resources internally.
 *       Automatically manages HTTP headers and follows redirects.
 * @see check_service_status()
 */
std::string CurlUtils::fetch_web_content(const std::string& url, std::optional<std::string> post_data,
                                         std::optional<std::string> custom_request_type,
                                         std::optional<std::map<std::string, std::string>> extra_headers,
                                         bool fail_on_http_error) {
    struct WebContent content {};
    std::string message{};

    this->initialize();
    this->spinner->start();

    curl_easy_setopt(this->curl, CURLOPT_URL, url.c_str());

    if (post_data != std::nullopt) {
        curl_easy_setopt(this->curl, CURLOPT_POSTFIELDS, post_data->c_str());
    }

    if (custom_request_type != std::nullopt) {
        curl_easy_setopt(this->curl, CURLOPT_CUSTOMREQUEST, custom_request_type->c_str());
    }

    curl_easy_setopt(this->curl, CURLOPT_WRITEFUNCTION, WriteCallback);
    curl_easy_setopt(this->curl, CURLOPT_WRITEDATA, &content.read_buffer);
    curl_easy_setopt(this->curl, CURLOPT_ERRORBUFFER, content.error_buffer);
    curl_easy_setopt(this->curl, CURLOPT_FOLLOWLOCATION, 1L);

    curl_easy_setopt(this->curl, CURLOPT_NOPROGRESS, 0L);
    curl_easy_setopt(this->curl, CURLOPT_XFERINFOFUNCTION, ProgressSpinner::progress_callback);
    curl_easy_setopt(this->curl, CURLOPT_XFERINFODATA, this->spinner.get());

    this->headers = curl_slist_append(this->headers, "Content-Type: application/json");

    if (extra_headers != std::nullopt) {
        for (const auto& [key, value] : extra_headers.value()) {
            std::string header = key + ": " + value;
            this->headers = curl_slist_append(this->headers, header.c_str());
        }
    }

    curl_easy_setopt(this->curl, CURLOPT_HTTPHEADER, this->headers);

    CURLcode result = curl_easy_perform(this->curl);
    this->spinner->stop();

    curl_easy_getinfo(this->curl, CURLINFO_RESPONSE_CODE, &content.status);

    this->cleanup();

    if (result != CURLE_OK) {
        message = std::format("CURL error: {}\n", curl_easy_strerror(result));
        if (this->ui->is_running() == false) {
            if (content.error_buffer[0] != '\0') {
                message += std::format("Details: {}\n", content.error_buffer);
            }
            UserInterface::print_error_noninteractive(message);
            exit(1);
        } else {
            this->ui->print_error(message);
            if (content.error_buffer[0] != '\0') {
                message = std::format("Details: {}\n", content.error_buffer);
                this->ui->print_error(message);
            }
            this->ui->end_session(EXIT_FAILURE);
        }
    }
    if (content.status != 200) {
        message = std::format("Request HTTP status: {}\n", content.status);
        if (this->ui->is_running() == false) {
            UserInterface::print_error_noninteractive(message);
            if (fail_on_http_error == true) {
                exit(1);
            } else {
                throw HttpException(content.status, url);
            }
        } else {
            this->ui->print_error(message);
            if (fail_on_http_error == true) {
                this->ui->end_session(EXIT_FAILURE);
            } else {
                throw HttpException(content.status, url);
            }
        }
    }

    return content.read_buffer;
}
