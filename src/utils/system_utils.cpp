#include "system_utils.hpp"
#include <format>

/**
 * Initializes a SystemUtils instance with a reference to the UserInterface.
 *
 * @param ui Pointer to UserInterface for displaying messages and error output.
 *           Must remain valid for the lifetime of this SystemUtils instance.
 *
 * @note The SystemUtils class does not take ownership of the ui pointer;
 *       the caller is responsible for managing the UserInterface lifetime.
 */
SystemUtils::SystemUtils(std::shared_ptr<UserInterface> ui) : ui(ui) {
}

/**
 * Reports a failure and ends the program with EXIT_FAILURE.
 *
 * In interactive mode, the message is shown through the UserInterface and the
 * session is ended with EXIT_FAILURE. Otherwise, the message is printed
 * non-interactively and the process exits immediately with EXIT_FAILURE.
 *
 * @param message The error message to display. The messages passed from
 *                catch_error each end with a newline.
 *
 * @note In interactive mode, this relies on UserInterface::end_session() not
 *       returning. If it does return, report_failure returns to its caller.
 */
void SystemUtils::report_failure(const std::string& message) {
    if (this->ui->backend_running()) {
        this->ui->print_error(message);
    } else {
        UserInterface::print_error_noninteractive(message);
    }
    this->ui->end_session(EXIT_FAILURE);
}

/**
 * Evaluates the wait status from a system() call and reports any failure.
 *
 * This method interprets the status with the POSIX macros WIFEXITED and
 * WEXITSTATUS. A status of -1 means system() itself failed, and only the
 * "system() failed." message is reported. A status where the process did not
 * exit normally is reported as "Command terminated abnormally." A normal exit
 * with a non-zero code is reported with that code. A successful command
 * (exit code 0) produces no output and returns normally.
 *
 * Each failure is passed to report_failure(), which shows the message through
 * the UserInterface in interactive mode or prints it non-interactively
 * otherwise, then ends the program with EXIT_FAILURE.
 *
 * @param status The wait status returned from a system() call, as interpreted
 *               by the POSIX macros WIFEXITED and WEXITSTATUS.
 *
 * @throws Does not throw. Ends the program with EXIT_FAILURE on any detected
 *         error, through report_failure().
 *
 * @note Non-interactive failures terminate the process immediately. Interactive
 *       failures return to the caller only if UserInterface::end_session() returns.
 */
void SystemUtils::catch_error(int status) {
    if (status == -1) {
        this->report_failure("system() failed.\n");
        return;
    }
    if (!WIFEXITED(status)) {
        this->report_failure("Command terminated abnormally.\n");
        return;
    }
    int exit_code = WEXITSTATUS(status);
    if (exit_code != 0) {
        this->report_failure(std::format("Command failed with exit code {}.\n", exit_code));
    }
}
