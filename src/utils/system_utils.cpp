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
SystemUtils::SystemUtils(UserInterface* ui) {
    this->ui = ui;
}

/**
 * Evaluates the status code from a system() call and handles errors appropriately.
 *
 * This method interprets POSIX wait status codes using WIFEXITED and WEXITSTATUS macros.
 * If an error is detected (status -1, non-zero exit code, or abnormal termination),
 * an error message is displayed via the UserInterface or printed non-interactively,
 * and the program exits with status code 1.
 *
 * A successful command (exit code 0) produces no output and returns normally.
 *
 * Error cases handled:
 * - system() call failure (status == -1): displays "system() failed" error.
 * - Non-zero exit code: displays the exit code and terminates the program.
 * - Abnormal termination: displays "Command terminated abnormally" and terminates the program.
 *
 * Output routing:
 * - If UserInterface is in interactive mode (is_running() == true), errors are sent via ui->print_error().
 * - Otherwise, errors are printed non-interactively via UserInterface::print_message_noninteractive().
 *
 * @param status The wait status code returned from a system() call.
 *               Interpret using POSIX macros WIFEXITED and WEXITSTATUS.
 *
 * @throws Terminates the program with exit code 1 on any detected error condition.
 *
 * @note This method never returns after encountering an error; callers should not
 *       expect execution to continue beyond a non-zero exit code or abnormal termination.
 */
void SystemUtils::catch_error(int status) {
    std::string message{};
    if (status == -1) {
        message = "system() failed.\n";
        if (this->ui->is_running() == false) {
            UserInterface::print_error_noninteractive(message);
        } else {
            this->ui->print_error(message);
        }
    }
    if (WIFEXITED(status)) {
        int exit_code = WEXITSTATUS(status);
        if (exit_code != 0) {
            message = std::format("Command failed with exit code {}.\n", exit_code);
            if (this->ui->is_running() == false) {
                UserInterface::print_error_noninteractive(message);
            } else {
                this->ui->print_error(message);
            }
            exit(1);
        }
    } else {
        message = std::format("Command terminated abnormally.\n");
        if (this->ui->is_running() == false) {
            UserInterface::print_error_noninteractive(message);
        } else {
            this->ui->print_error(message);
        }
        exit(1);
    }
}
