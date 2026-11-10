#pragma once

#include <exception>

/**
 * Exception thrown to request that the application exit with a specific code.
 *
 * Throwing an ExitRequest signals that the program should shut down in an
 * orderly way. The exit code travels with the exception, so the top-level
 * handler can pass it to the operating system. Because the class derives from
 * std::exception, a catch block for std::exception will also match it, so
 * handlers that must let it through should catch ExitRequest first.
 *
 * Catch it by const reference. The constructor, code(), and what() are all
 * noexcept, so the object can be created and inspected without risk of
 * throwing a second exception.
 */
class ExitRequest : public std::exception {
  private:
    /** The exit code the application should return to the operating system. */
    const int exit_code;

    std::string exit_reason{};

  public:
    /**
     * Constructs an exit request with the given exit code.
     *
     * The constructor is explicit, so an int cannot be implicitly converted to
     * an ExitRequest. The exit request must be created deliberately.
     *
     * @param code The exit code to report when the application terminates.
     */
    explicit ExitRequest(const int code, const std::string& reason = "application exit requested") noexcept
        : exit_code(code), exit_reason(reason) {
    }

    /**
     * Returns the exit code carried by this request.
     *
     * @return The exit code passed to the constructor. This function does not
     *         throw.
     */
    int code() const noexcept {
        return this->exit_code;
    }

    /**
     * Returns a description of the exception.
     *
     * The message is fixed and does not include the exit code. Use code() to
     * retrieve the code. The returned pointer refers to a string literal with
     * static storage duration, so it remains valid for the lifetime of the
     * program.
     *
     * @return A null-terminated string, "application exit requested".
     */
    const char* what() const noexcept override {
        return this->exit_reason.c_str();
    }
};
