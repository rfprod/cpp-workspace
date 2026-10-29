#include <format>
#include <stdexcept>
#include <string>

/**
 * @brief Exception thrown when an HTTP request fails with a non-2xx status code or CURL error.
 *
 * Inherits from std::runtime_error and stores the HTTP status code for detailed error handling.
 * Allows calling code to distinguish between different HTTP errors (e.g., 404 vs 500) and handle
 * them appropriately.
 *
 * @see CurlUtils::fetch_web_content()
 *
 * @example
 * @code
 * try {
 *     std::string response = curl->fetch_web_content(url);
 * } catch (const HttpException& e) {
 *     if (e.get_status_code() == 404) {
 *         std::cout << "Resource not found" << std::endl;
 *     } else {
 *         std::cout << "HTTP error: " << e.what() << std::endl;
 *     }
 * }
 * @endcode
 */
class HttpException : public std::runtime_error {
  private:
    /**
     * The HTTP status code that triggered the exception.
     */
    long status_code;

  public:
    /**
     * @brief Constructs an HttpException with HTTP status code and URL information.
     *
     * The exception message is formatted as "HTTP {status_code}: {url}" and is accessible
     * via std::exception::what().
     *
     * @param status_code The HTTP status code (e.g., 404, 500). Pass 0 for CURL errors.
     * @param url The URL that was requested when the error occurred.
     *
     * @example
     * @code
     * throw HttpException(404, "http://localhost:6333/collections/missing");
     * @endcode
     */
    HttpException(long status_code, const std::string& url)
        : std::runtime_error(std::format("HTTP {}: {}", status_code, url)), status_code(status_code) {
    }

    /**
     * @brief Retrieves the HTTP status code associated with this exception.
     *
     * Useful for determining whether an error is recoverable (e.g., 404 means resource doesn't exist)
     * or critical (e.g., 500 means server error).
     *
     * @return The HTTP status code as a long. Returns 0 for CURL connection errors.
     *
     * @example
     * @code
     * catch (const HttpException& e) {
     *     if (e.get_status_code() == 404) {
     *         // Handle not found
     *     }
     * }
     * @endcode
     */
    long get_status_code() const {
        return status_code;
    }
};
