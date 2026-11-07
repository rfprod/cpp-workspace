#include "curl_utils.hpp"
#include <format>
#include <string>

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
CurlUtils::CurlUtils(std::shared_ptr<UserInterface> ui) : ui(ui) {
    this->curl = nullptr;
    this->headers = nullptr;
    this->spinner = std::make_unique<ProgressSpinner>(ui, false);
}

/**
 * Destroys the CurlUtils instance and releases its CURL resources.
 *
 * Calls cleanup(), which resets the curl and headers unique_ptrs and runs their
 * deleters. Because the unique_ptrs also release their resources when they are
 * destroyed, this call is not strictly required. It is kept so that the handle
 * is released in the same way as at the end of each request.
 *
 * @note cleanup() also prints a completion message. This output appears when the
 *       object is destroyed, even if no request was made.
 */
CurlUtils::~CurlUtils() {
    this->cleanup();
}

/**
 * Creates a new CURL easy handle and stores it in the curl unique_ptr.
 *
 * Calls curl_easy_init() and assigns the result with reset(), which releases any
 * previously held handle through its custom deleter (curl_easy_cleanup). If
 * initialization fails, the error is reported through the UI or to stdout, and
 * the process exits (non-interactive mode) or the UI session ends with
 * EXIT_FAILURE. On success, a confirmation message is printed.
 *
 * @note Because reset() releases an existing handle, calling this method
 *       discards any state set on the previous handle.
 */
void CurlUtils::initialize() {
    this->curl.reset(curl_easy_init());
    std::string message{};
    if (!this->curl) {
        message = "Failed to initialize CURL.\n";
        if (UserInterface::ui_running(this->ui) == false) {
            UserInterface::print_error_noninteractive(message);
            std::exit(EXIT_FAILURE);
        } else {
            this->ui->print_error(message);
            this->ui->end_session(EXIT_FAILURE);
        }
    } else {
        message = "CURL is initialized.\n";
        if (UserInterface::ui_running(this->ui) == false) {
            UserInterface::print_success_noninteractive(message);
        } else {
            this->ui->print_success(message);
        }
    }
}

/**
 * Releases the CURL handle and the header list held by this object.
 *
 * Calls reset() on the curl and headers unique_ptrs, which runs their custom
 * deleters (curl_easy_cleanup and curl_slist_free_all). Each resource is only
 * released if it is currently held, so calling this method more than once is safe.
 *
 * In DEBUG builds, the pointer values are logged before they are released when
 * no UI is running. A success message is printed at the end, either through the
 * UI or to stdout.
 *
 * @note Do not call curl_easy_cleanup or curl_slist_free_all manually for these
 *       resources. Doing so together with the deleters causes a double free.
 */
void CurlUtils::cleanup() {
    if (this->curl) {
#ifdef DEBUG
        if (UserInterface::ui_running(this->ui) == false) {
            UserInterface::print_message_noninteractive(
                std::format("\t[DEBUG] curl pointer before cleanup: {}\n", static_cast<void*>(this->curl.get())));
        }
#endif
        this->curl.reset();
    }
    if (this->headers) {
#ifdef DEBUG
        if (UserInterface::ui_running(this->ui) == false) {
            UserInterface::print_message_noninteractive(std::format(
                "\t[DEBUG] curl headers pointer before cleanup: {}\n", static_cast<void*>(this->headers.get())));
        }
#endif
        this->headers.reset();
    }
    std::string message = "CURL cleanup is complete.\n";
    if (UserInterface::ui_running(this->ui) == false) {
        UserInterface::print_success_noninteractive(message);
    } else {
        this->ui->print_success(message);
    }
}

/**
 * Appends a single header line to a curl_slist that is being built.
 *
 * Calls curl_slist_append() and, on success, updates the caller's list pointer
 * to the new head. If the append fails, the pointer is left unchanged, so the
 * previously built list remains valid and is not lost.
 *
 * @param headers Reference to the curl_slist pointer being built. May be nullptr
 *                to start a new list.
 * @param line The header line to append, in the form "Name: value".
 *
 * @note On failure the header is silently dropped. Callers that must guarantee
 *       every header is sent should check the list afterwards.
 */
void CurlUtils::add_header(curl_slist*& headers, const std::string& line) {
    curl_slist* updated = curl_slist_append(headers, line.c_str());
    if (updated != nullptr) {
        headers = updated;
    }
}

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
size_t CurlUtils::curl_write_callback(char* contents, size_t size, size_t nmemb, std::string* userp) {
    userp->append(contents, size * nmemb);
    return size * nmemb;
}

/**
 * CURL write callback for streaming response data.
 *
 * Called by CURL whenever a chunk of the response body is received. The chunk
 * is copied into a std::string and passed to CurlUtils::process_chunk() on the
 * CurlUtils instance stored in userp. Output for errors and debug messages goes
 * through the UserInterface when it is running, and to stdout or stderr
 * otherwise.
 *
 * @param contents Pointer to the received data chunk. Not null-terminated.
 * @param size Size of each element (always 1 for this use case).
 * @param nmemb Number of elements received.
 * @param userp Pointer to the CurlUtils instance that owns the transfer.
 *
 * @return Total bytes processed (size * nmemb). Returns 0 if process_chunk()
 *         throws, which makes CURL abort the transfer with CURLE_WRITE_ERROR.
 *
 * @note This method reads curl_obj->ui directly, so it is unsafe when no
 *       UserInterface is set. Consider using ui_running() before each access.
 */
size_t CurlUtils::curl_streaming_write_callback(char* contents, size_t size, size_t nmemb, void* userp) {
    size_t realsize = size * nmemb;

    CurlUtils* curl_obj = static_cast<CurlUtils*>(userp);

#ifdef DEBUG
    if (UserInterface::ui_running(curl_obj->ui) == false) {
        UserInterface::print_message_noninteractive(
            std::format("\t[DEBUG] 🔗 curl_streaming_write_callback called, size: {}\n", realsize));
    }
#endif

    try {
        std::string chunk(contents, realsize);
        curl_obj->process_chunk(chunk);
    } catch (const std::exception& e) {
        std::string message = std::format("Exception in curl_streaming_write_callback: {}", e.what());
        if (UserInterface::ui_running(curl_obj->ui) == false) {
            UserInterface::print_error_noninteractive(message);
        } else {
            curl_obj->ui->print_error(message);
        }
        // Returning 0 aborts the transfer
        return 0;
    }

    return realsize;
}

/**
 * Buffers streamed response data and dispatches complete JSON lines.
 *
 * Appends the incoming chunk to stream_buffer and extracts every complete line
 * (terminated by '\n'). A trailing '\r' is removed, so \r\n line endings work.
 * Only lines that start with '{' are treated as JSON and passed to
 * stream_callback. Other lines are skipped. Any incomplete trailing data stays
 * in stream_buffer until the next chunk arrives. If no stream_callback is set,
 * the line is dropped.
 *
 * Debug output goes to stdout only when no UserInterface is running.
 *
 * @param chunk The raw data received from the current CURL write callback. It
 *              may contain zero, one, or several complete lines, or a partial line.
 *
 * @note Each complete line is passed to stream_callback as a copy, so the
 *       callback may keep it. Lines that are not JSON objects are discarded.
 */
void CurlUtils::process_chunk(const std::string& chunk) {
#ifdef DEBUG
    if (UserInterface::ui_running(this->ui) == false) {
        UserInterface::print_message_noninteractive(
            std::format("\t[DEBUG] 🔥 process_chunk called with {}\n", chunk.size()));
    }
#endif

    this->stream_buffer += chunk;

    // Extract complete lines (ending with \n).
    size_t pos = 0;
    while ((pos = this->stream_buffer.find('\n')) != std::string::npos) {
        std::string line = this->stream_buffer.substr(0, pos);
        this->stream_buffer.erase(0, pos + 1);

        // Trim trailing \r (HTTP line endings are \r\n).
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }

#ifdef DEBUG
        if (UserInterface::ui_running(this->ui) == false) {
            UserInterface::print_message_noninteractive(std::format(
                "\t[DEBUG] 🔥 Extracted line: {} {}\n", line.substr(0, 80), (line.size() > 80 ? "..." : "")));
        }
#endif

        if (line.empty() || line[0] != '{') {
#ifdef DEBUG
            if (UserInterface::ui_running(this->ui) == false) {
                UserInterface::print_message_noninteractive(std::format("\t[DEBUG] 🔥 Skipping non-JSON line.\n"));
            }
#endif
            continue;
        }

        // Pass only valid JSON lines to the callback.
        if (stream_callback) {
#ifdef DEBUG
            if (UserInterface::ui_running(this->ui) == false) {
                UserInterface::print_message_noninteractive(std::format("\t[DEBUG] 🔥 Calling stream_callback.\n"));
            }
#endif
            stream_callback(line);
        } else {
#ifdef DEBUG
            if (UserInterface::ui_running(this->ui) == false) {
                UserInterface::print_message_noninteractive("⚠️  stream_callback is NULL!\n");
            }
#endif
        }
    }
    // Remaining incomplete data stays in stream_buffer for next chunk.
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

    curl_easy_setopt(this->curl.get(), CURLOPT_URL, url.c_str());

    curl_easy_setopt(this->curl.get(), CURLOPT_CUSTOMREQUEST, "GET");

    curl_easy_setopt(this->curl.get(), CURLOPT_WRITEFUNCTION,
                     [](void*, size_t size, size_t nmemb, void*) { return size * nmemb; });
    curl_easy_setopt(this->curl.get(), CURLOPT_TIMEOUT, timeout_seconds);
    curl_easy_setopt(this->curl.get(), CURLOPT_CONNECTTIMEOUT, timeout_seconds);
    curl_easy_setopt(this->curl.get(), CURLOPT_FOLLOWLOCATION, 1L);

    curl_easy_setopt(this->curl.get(), CURLOPT_NOPROGRESS, 0L);
    curl_easy_setopt(this->curl.get(), CURLOPT_XFERINFOFUNCTION, ProgressSpinner::progress_callback);
    curl_easy_setopt(this->curl.get(), CURLOPT_XFERINFODATA, this->spinner.get());

    curl_slist* new_headers = nullptr;
    this->add_header(new_headers, "Content-Type: application/json");

    for (const auto& [key, value] : extra_headers) {
        this->add_header(new_headers, key + ": " + value);
    }
    this->headers.reset(new_headers);

    curl_easy_setopt(this->curl.get(), CURLOPT_HTTPHEADER, this->headers.get());

    CURLcode res = curl_easy_perform(this->curl.get());
    this->spinner->stop();

    curl_easy_getinfo(this->curl.get(), CURLINFO_RESPONSE_CODE, &http_code);

    this->cleanup();

    if (res != CURLE_OK) {
        std::string message = std::format("Curl error: {}\n", curl_easy_strerror(res));
        if (UserInterface::ui_running(this->ui) == false) {
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
            message = std::format("Service available (HTTP {}) after {} attempt(s)\n", http_code, attempt + 1);
            if (UserInterface::ui_running(this->ui) == false) {
                UserInterface::print_success_noninteractive(message);
            } else {
                this->ui->print_success(message);
            }
            return http_code;
        }

        if (attempt < max_retries - 1) {
            message = std::format("⏳ Waiting for service... (attempt {}/{}, retrying in {}ms)\n", attempt + 1,
                                  max_retries, delay_ms);
            if (UserInterface::ui_running(this->ui) == false) {
                UserInterface::print_message_noninteractive(message);
            } else {
                this->ui->print_message(message);
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(delay_ms));
            delay_ms = std::min(delay_ms * 2, 5000);
        }
    }

    message = std::format("Service unavailable after {} attempts\n", max_retries);
    if (UserInterface::ui_running(this->ui) == false) {
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
 * @throws Calls std::exit(EXIT_FAILURE) on CURL errors or non-200 HTTP status codes
 *         if the UI is not running. If UI is running, terminates the
 *         session with EXIT_FAILURE.
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

    curl_easy_setopt(this->curl.get(), CURLOPT_URL, url.c_str());

    if (post_data != std::nullopt) {
        curl_easy_setopt(this->curl.get(), CURLOPT_POSTFIELDS, post_data->c_str());
    }

    if (custom_request_type != std::nullopt) {
        curl_easy_setopt(this->curl.get(), CURLOPT_CUSTOMREQUEST, custom_request_type->c_str());
    }

    curl_easy_setopt(this->curl.get(), CURLOPT_WRITEFUNCTION, CurlUtils::curl_write_callback);
    curl_easy_setopt(this->curl.get(), CURLOPT_WRITEDATA, &content.read_buffer);
    curl_easy_setopt(this->curl.get(), CURLOPT_ERRORBUFFER, content.error_buffer);
    curl_easy_setopt(this->curl.get(), CURLOPT_FOLLOWLOCATION, 1L);

    curl_easy_setopt(this->curl.get(), CURLOPT_NOPROGRESS, 0L);
    curl_easy_setopt(this->curl.get(), CURLOPT_XFERINFOFUNCTION, ProgressSpinner::progress_callback);
    curl_easy_setopt(this->curl.get(), CURLOPT_XFERINFODATA, this->spinner.get());

    curl_slist* new_headers = nullptr;
    this->add_header(new_headers, "Content-Type: application/json");

    if (extra_headers != std::nullopt) {
        for (const auto& [key, value] : extra_headers.value()) {
            this->add_header(new_headers, key + ": " + value);
        }
    }
    this->headers.reset(new_headers);

    curl_easy_setopt(this->curl.get(), CURLOPT_HTTPHEADER, this->headers.get());

    CURLcode result = curl_easy_perform(this->curl.get());
    this->spinner->stop();

    curl_easy_getinfo(this->curl.get(), CURLINFO_RESPONSE_CODE, &content.status);

    this->cleanup();

    if (result != CURLE_OK) {
        message = std::format("CURL error: {}\n", curl_easy_strerror(result));
        if (UserInterface::ui_running(this->ui) == false) {
            if (content.error_buffer[0] != '\0') {
                message += std::format("Details: {}\n", content.error_buffer);
            }
            UserInterface::print_error_noninteractive(message);
            std::exit(EXIT_FAILURE);
        } else {
            this->ui->print_error(message);
            if (content.error_buffer[0] != '\0') {
                message = std::format("Details: {}\n", content.error_buffer);
                this->ui->print_error(message);
            }
            this->ui->end_session(EXIT_FAILURE);
        }
    } else if (content.status != 200) {
        message = std::format("Request HTTP status: {}\n", content.status);
        if (UserInterface::ui_running(this->ui) == false) {
            UserInterface::print_error_noninteractive(message);
            if (fail_on_http_error == true) {
                std::exit(EXIT_FAILURE);
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

/**
 * Performs an HTTP request and streams the response body to a callback.
 *
 * Stores on_chunk as the stream callback, then sends the request through the
 * shared curl handle. The response body is passed through
 * curl_streaming_write_callback and process_chunk, which deliver each complete
 * JSON line to on_chunk as it arrives. Unlike fetch_web_content(), this method
 * does not start the progress spinner.
 *
 * Content-Type: application/json is always sent. extra_headers are added after it.
 * When the transfer finishes, the handle and headers are released through
 * cleanup(). The returned WebContentStream reports whether the request succeeded,
 * along with the HTTP status and any error message.
 *
 * On a CURL error or a non-200 status, the error is shown through the UI or to
 * stdout and stderr. The process then exits (non-interactive mode, or when
 * fail_on_http_error is true for an HTTP error) or the UI session ends with
 * EXIT_FAILURE. When fail_on_http_error is false, an HTTP error throws an
 * HttpException.
 *
 * @param url The request URL.
 * @param post_data Optional request body, sent with CURLOPT_POSTFIELDS.
 * @param on_chunk Callback invoked once for each complete JSON line in the response.
 * @param custom_request_type Optional HTTP method override, such as "DELETE".
 * @param extra_headers Optional additional headers, as name and value pairs.
 * @param fail_on_http_error If true, a non-200 status ends the program instead of throwing.
 *
 * @return A WebContentStream with the HTTP status, the success flag, and the error message.
 *
 * @throws HttpException If the HTTP status is not 200 and fail_on_http_error is false.
 *
 * @note The post_data string must stay valid until curl_easy_perform() returns.
 *       content.status must be a long, because CURLINFO_RESPONSE_CODE writes a long.
 */
WebContentStream CurlUtils::fetch_web_content_stream(const std::string& url, std::optional<std::string> post_data,
                                                     std::function<void(const std::string&)> on_chunk,
                                                     std::optional<std::string> custom_request_type,
                                                     std::optional<std::map<std::string, std::string>> extra_headers,
                                                     bool fail_on_http_error) {
    this->stream_callback = on_chunk;

    struct WebContentStream content {};
    std::string message{};

    this->initialize();
    this->stream_buffer.clear();

    curl_easy_setopt(this->curl.get(), CURLOPT_URL, url.c_str());

    if (post_data != std::nullopt) {
        curl_easy_setopt(this->curl.get(), CURLOPT_POSTFIELDS, post_data->c_str());
    }

    if (custom_request_type != std::nullopt) {
        curl_easy_setopt(this->curl.get(), CURLOPT_CUSTOMREQUEST, custom_request_type->c_str());
    }

    curl_easy_setopt(this->curl.get(), CURLOPT_WRITEFUNCTION, CurlUtils::curl_streaming_write_callback);
    curl_easy_setopt(this->curl.get(), CURLOPT_WRITEDATA, static_cast<void*>(this));
    curl_easy_setopt(this->curl.get(), CURLOPT_ERRORBUFFER, content.error_buffer);
    curl_easy_setopt(this->curl.get(), CURLOPT_FOLLOWLOCATION, 1L);

    curl_slist* new_headers = nullptr;
    this->add_header(new_headers, "Content-Type: application/json");

    if (extra_headers != std::nullopt) {
        for (const auto& [key, value] : extra_headers.value()) {
            this->add_header(new_headers, key + ": " + value);
        }
    }

    this->headers.reset(new_headers);

    curl_easy_setopt(this->curl.get(), CURLOPT_HTTPHEADER, this->headers.get());

    CURLcode result = curl_easy_perform(this->curl.get());

    curl_easy_getinfo(this->curl.get(), CURLINFO_RESPONSE_CODE, &content.status);

    this->cleanup();

    content.success = true;
    if (result != CURLE_OK) {
        content.success = false;
        content.error_message = curl_easy_strerror(result);
        message = std::format("CURL error: {}\n", curl_easy_strerror(result));
        if (UserInterface::ui_running(this->ui) == false) {
            if (content.error_buffer[0] != '\0') {
                message += std::format("Details: {}\n", content.error_buffer);
            }
            UserInterface::print_error_noninteractive(message);
            std::exit(EXIT_FAILURE);
        } else {
            this->ui->print_error(message);
            if (content.error_buffer[0] != '\0') {
                message = std::format("Details: {}\n", content.error_buffer);
                this->ui->print_error(message);
            }
            this->ui->end_session(EXIT_FAILURE);
        }
    } else if (content.status != 200) {
        content.success = false;
        content.error_message = std::format("HTTP {}", content.status);
        message = std::format("Request HTTP status: {}\n", content.status);
        if (UserInterface::ui_running(this->ui) == false) {
            UserInterface::print_error_noninteractive(message);
            if (fail_on_http_error == true) {
                std::exit(EXIT_FAILURE);
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

    return content;
}
