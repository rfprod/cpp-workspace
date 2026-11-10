#include "user_interface.hpp"
#include "utils/exit_request.hpp"
#include <chrono>
#include <csignal>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <locale>
#include <sstream>

std::weak_ptr<UserInterface> UserInterface::instance;

/**
 * Creates a UserInterface instance with the light theme as the default color scheme.
 *
 * @return: Shared pointer to the newly created UserInterface, also stored in the static instance member.
 *
 * Delegates to create(bool) with dark_theme set to false, so use_dark_theme is initialized to the light theme.
 * Any previously stored instance is replaced by the new one.
 *
 * Note: This overload does not call init_ncurses(). The caller must invoke init_ncurses() explicitly on the returned
 * instance to set up the terminal, color pairs, and input handling.
 */
std::shared_ptr<UserInterface> UserInterface::create() {
    return create(false);
}

/**
 * Creates a UserInterface instance with a caller-specified color theme preference and registers it as the singleton
 * instance.
 *
 * @param dark_theme: Boolean flag to select color scheme; true for dark theme, false for light theme.
 * @return: Shared pointer to the newly created UserInterface, also stored in the static instance member.
 *
 * Allocates a new UserInterface through the constructor, passing dark_theme so that use_dark_theme is set during
 * construction. Assigns the new object to instance, replacing any previously stored value. Returns the shared pointer
 * to the caller.
 *
 * Note: This method does not call init_ncurses(). The caller must invoke init_ncurses() explicitly on the returned
 * instance to set up the terminal, color pairs, and input handling.
 */
std::shared_ptr<UserInterface> UserInterface::create(bool dark_theme) {
    std::shared_ptr<UserInterface> ui(new UserInterface(dark_theme));
    instance = ui;
    return ui;
}

/**
 * Constructs a UserInterface instance and initializes member variables.
 *
 * Initializes the TerminalBackend.
 */
UserInterface::UserInterface() {
    this->backend = std::make_unique<TerminalBackend>();
}

/**
 * Constructs a UserInterface instance with a caller-specified color theme preference.
 *
 * @param dark_theme: Boolean flag to select color scheme; true for dark theme, false for light theme.
 *
 * Initializes the TerminalBackend.
 */
UserInterface::UserInterface(bool dark_theme) {
    this->backend = std::make_unique<TerminalBackend>(dark_theme);
}

/**
 * Destructs a UserInterface instance and releases ncurses resources.
 */
UserInterface::~UserInterface() {
    this->end_session();
}

/**
 * Reports whether the backend's thread is currently running.
 *
 * The function returns the result of backend->is_running(). It does not
 * acquire ui_mutex, and the value can change immediately after the call
 * returns.
 *
 * @return true if the backend thread is running, false otherwise.
 *
 * @pre backend is not null.
 *
 * @note Use this for status checks only. Do not use it to decide whether to
 *       start or stop the backend without coordinating through another
 *       synchronization mechanism, because the state can change between the
 *       check and the action.
 */
bool UserInterface::backend_running() {
    return this->backend->is_running();
}

/**
 * Runs the user interface by delegating to the backend.
 *
 * The function calls backend->run() and does nothing else. The behavior,
 * including whether and when the call returns, is defined by
 * TerminalBackend::run().
 *
 * @pre backend is not null.
 *
 * @note Any blocking, error handling, or session termination happens inside
 *       TerminalBackend::run(). Refer to that method's documentation for those
 *       details.
 */
void UserInterface::run(void) {
    this->backend->run();
}

/**
 * Shows a session-ended message, ends the backend session.
 *
 * The function calls print_message() with the end-of-session message, calls
 * backend->end_session() to stop the render thread and restore the terminal,
 * and then calls std::exit(code). The function does not return.
 *
 * @pre backend is not null.
 */
void UserInterface::end_session(int code) {
    this->print_message("🏁 The session has ended.");

    if (this->backend->is_running()) {
        this->backend->end_session(code);
    } else {
        throw ExitRequest{code};
    }
}

/**
 * Displays runtime configuration arguments as formatted diagnostic output.
 *
 * @param model Name of the primary language model in use.
 * @param embedding_model Name of the embedding model in use.
 * @param prompt Initial prompt or query string provided by the user.
 * @param stream Boolean flag indicating whether streaming mode is enabled.
 * @param rag Whether to run the program in the Retrieval Augmented Generation mode.
 * @param interactive Boolean flag indicating whether interactive mode is enabled.
 * @param dark_theme Boolean flag indicating whether dark theme is active.
 * @param skip_embedding Whether to skip vectorizing documents and creating embeddings.
 *
 * Constructs a multi-line diagnostic string containing all provided configuration parameters.
 * Each parameter is formatted with a 🛈 (information) icon prefix for visual consistency.
 * Boolean parameters are converted to human-readable "true" or "false" strings using conditional
 * operators, then concatenated into the buffer.
 *
 * Prints the formatted buffer via this->print_message(), displaying
 * the configuration summary in the output area for user visibility.
 *
 * @return void
 *
 * Output is displayed via print() and committed to the output buffer.
 *
 * Acts as a diagnostic utility to confirm application startup parameters and aid in
 * troubleshooting configuration issues. Typically called early during initialization to
 * provide user confirmation of active settings.
 *
 * Enables transparency into runtime configuration and helps users verify that the correct
 * models, modes, and theme are in effect.
 */
void UserInterface::print_args(const std::string& model, const std::string& embedding_model, const std::string& prompt,
                               bool stream, bool rag, bool interactive, bool dark_theme, bool skip_embedding) {
    std::string buffer;
    buffer += "🛈 model = " + model + "\n";
    buffer += "🛈 embedding_model = " + embedding_model + "\n";
    buffer += "🛈 prompt = " + prompt + "\n";
    buffer += "🛈 stream = " + std::string(stream ? "true" : "false") + "\n";
    buffer += "🛈 rag = " + std::string(rag ? "true" : "false") + "\n";
    buffer += "🛈 interactive = " + std::string(interactive ? "true" : "false") + "\n";
    buffer += "🛈 dark_theme = " + std::string(dark_theme ? "true" : "false") + "\n";
    buffer += "🛈 skip_embedding = " + std::string(skip_embedding ? "true" : "false") + "\n";

    this->print_message(buffer);
}

/**
 * @brief Displays runtime configuration arguments to stdout in non-interactive mode.
 *
 * @param model Name of the primary language model in use.
 * @param embedding_model Name of the embedding model in use.
 * @param prompt Initial prompt or query string provided by the user.
 * @param stream Boolean flag indicating whether streaming mode is enabled.
 * @param rag Whether to run the program in the Retrieval Augmented Generation mode.
 * @param interactive Boolean flag indicating whether interactive mode is enabled.
 * @param dark_theme Boolean flag indicating whether dark theme is active.
 * @param skip_embedding Whether to skip vectorizing documents and creating embeddings.
 *
 * @return void
 *
 * Outputs runtime configuration to standard output (stdout) using printf(), bypassing the
 * ncurses UI buffer. This method is used when the application runs in non-interactive mode,
 * where the terminal UI is not initialized and direct console output is required.
 *
 * Constructs and prints a multi-line diagnostic string containing all provided configuration
 * parameters. Each parameter is formatted with a 🛈 (information) icon prefix for visual
 * consistency with interactive mode output. Boolean parameters are converted to human-readable
 * "true" or "false" strings using conditional operators.
 *
 * Includes leading and trailing newlines (printf("\n")) to visually separate the configuration
 * summary from other console output, improving readability in log files and terminal sessions.
 *
 * Acts as the non-interactive counterpart to print_args(). Enables configuration transparency
 * and aids in troubleshooting when the application runs in batch or command-line mode without
 * the interactive terminal UI.
 *
 * @note This method uses direct printf() calls and should only be used before ncurses
 * initialization or after cleanup, to avoid conflicts with the ncurses output buffer.
 */
void UserInterface::print_args_noninteractive(const std::string& model, const std::string& embedding_model,
                                              const std::string& prompt, bool stream, bool rag, bool interactive,
                                              bool dark_theme, bool skip_embedding) {
    printf("\n");
    printf("🛈 model = %s\n", model.c_str());
    printf("🛈 embedding_model = %s\n", embedding_model.c_str());
    printf("🛈 prompt = %s\n", prompt.c_str());
    printf("🛈 stream = %s\n", stream ? "true" : "false");
    printf("🛈 rag = %s\n", rag ? "true" : "false");
    printf("🛈 interactive = %s\n", interactive ? "true" : "false");
    printf("🛈 dark_theme = %s\n", dark_theme ? "true" : "false");
    printf("🛈 skip_embedding = %s\n", skip_embedding ? "true" : "false");
    printf("\n");
}

/**
 * @brief Displays a numbered list of available models to the user via the UI buffer.
 *
 * @param models A constant reference to a vector of model name strings to be displayed.
 *
 * @return void
 *
 * Renders an interactive model selection menu by formatting each model name with its
 * zero-indexed position. This method is used during interactive sessions to present
 * available Ollama models to the user, enabling selection via numeric input.
 *
 * Constructs a formatted string buffer containing an information message ("🛈 Select a
 * model from the list below:") followed by a numbered list. Each entry is formatted as
 * "[index] model_name" on a separate line, where index is zero-based and automatically
 * converted from the loop counter using std::to_string().
 *
 * Outputs the constructed buffer to the ncurses UI using print_message() styling,
 * ensuring the model list appears in the output area and is preserved in the UI's
 * output_buffer for proper rendering and scrolling.
 *
 * @note This method assumes models is not empty; calling with an empty vector will display
 * only the header message without any selectable options. Typically called after successfully
 * retrieving the model list from the Ollama server via query_model_list().
 */
void UserInterface::print_model_list(const std::vector<std::string>& models) {
    std::string buffer;
    buffer += "\n🛈 Select a model from the list below:\n";
    for (size_t i = 0; i < models.size(); i += 1) {
        buffer += "[" + std::to_string(i) + "] " + models[i] + "\n";
    }

    this->print_message(buffer);
}

/**
 * @brief Outputs a numbered list of available models to stdout in non-interactive mode.
 *
 * @param models A constant reference to a vector of model name strings.
 *
 * @return void
 *
 * Formats and prints each model with its zero-indexed position as "[index] model_name"
 * using printf() directly, bypassing the ncurses UI buffer. Called when the application
 * runs in non-interactive mode and the terminal UI is not initialized.
 *
 * @note This method should only be used before ncurses initialization or after cleanup.
 */
void UserInterface::print_model_list_noninteractive(const std::vector<std::string>& models) {
    printf("\n🛈 Select a model from the list below:\n");
    for (size_t i = 0; i < models.size(); i += 1) {
        printf("[%zu] %s\n", i, models[i].c_str());
    }
}

/**
 * @brief Prompts the user to select a model by numeric index via interactive keyboard input.
 *
 * @param models A constant reference to a vector of available model name strings.
 * @param model A pointer to a string where the selected model name will be stored.
 *
 * @return void
 *
 * Validates that models is not empty; exits with error if no models are available.
 * Clears the input buffer and enters a getch() loop to capture user input, supporting
 * backspace (KEY_BACKSPACE or 127) and printable characters with length constraints.
 * Input terminates on newline (Enter key).
 *
 * Interprets input as a zero-indexed model position via stoi(). If input is "q" or "Q",
 * terminates the session. If the index is out of bounds, prints an error and recursively
 * re-prompts. If valid, dereferences model and assigns the selected model name.
 *
 * @note Input buffer length is capped at COLS - 40 to prevent overflow. Invalid indices
 * trigger recursive retry. Quit commands (q/Q) terminate the entire session.
 */
void UserInterface::select_model(const std::vector<std::string>& models, std::string* model) {
    int index{};

    if (models.size() == 0) {
        this->print_error("There are no available models to select from.\n");
        this->end_session(EXIT_FAILURE);
    }

    this->request_redraw();

    this->backend->clear_input_buffer();
    int ch;

    while ((ch = getch()) != '\n') {
        if (ch == KEY_BACKSPACE || ch == 127) {
            if (!this->backend->input_buffer_is_empty()) {
                this->backend->pop_back_input_buffer();
                this->backend->shift_input_cursor_x(-1);
                this->request_redraw();
            }
        } else if (ch != ERR && isprint(ch)) {
            if (this->backend->room_for_prompt_in_input_buffer()) {
                this->backend->push_back_input_buffer(ch);
                this->backend->shift_input_cursor_x(1);
                this->request_redraw();
            }
        }
    }

    if (this->backend->exit_keys_in_input_buffer()) {
        this->end_session(EXIT_SUCCESS);
    }

    index = stoi(std::string(this->backend->get_input_buffer()));

    if (index < 0 || index >= models.size()) {
        std::string message = std::format("Model index {} does not exist.\n", index);
        this->print_error(message);
        this->select_model(models, model);
    } else {
        *model = models[index];
    }
}

/**
 * @brief Prompts for a model selection via stdin in non-interactive mode.
 *
 * @param models A constant reference to a vector of available model name strings.
 * @param model A pointer to a string where the selected model name will be stored.
 *
 * @return void
 *
 * Validates that models is not empty; exits with error if none are available.
 * Loops until a valid zero-indexed model position is successfully read from stdin via scanf().
 * Repeats the prompt if scanf() fails to read exactly one integer or if the index is out of bounds.
 * Prints validation errors to stderr. On valid input, dereferences model and assigns the
 * selected model name.
 *
 * @note Uses stderr for error messages and stdout for prompts. Loop continues until
 * scanf() succeeds and index is within [0, models.size()).
 */
void UserInterface::select_model_noninteractive(const std::vector<std::string>& models, std::string* model) {
    int index{};
    int read_result{};

    if (models.size() == 0) {
        UserInterface::print_error_noninteractive("There are no available models to select from.\n");
        if (auto ui = instance.lock()) {
            ui->end_session(EXIT_FAILURE);
        }
    }

    while (read_result != 1 || index < 0 || index >= models.size()) {
        if (index < 0 || index >= models.size()) {
            UserInterface::print_error_noninteractive(std::format("Model index {} does not exist.\n", index));
        }
        UserInterface::print_message_noninteractive("Model index: ");
        read_result = scanf("%d", &index);
    }

    *model = models[index];
}

/**
 * Reads one line of user input into the prompt and reports whether it is an
 * exit request.
 *
 * The function requests a redraw, clears the input buffer, and then reads keys
 * with getch() until Enter ('\n') is pressed. Backspace removes the last
 * character and moves the cursor left. A printable character is appended only
 * if room_for_prompt_in_input_buffer() is true, and it moves the cursor right.
 * Each change requests a redraw. When Enter is pressed, the buffer contents are
 * copied into *prompt. The function returns true if the text is exactly "q" or
 * "Q".
 *
 * @param prompt Output string that receives the entered text. Must not be null.
 *
 * @return true if the entered text is "q" or "Q", otherwise false.
 *
 * @pre prompt is not null.
 * @post *prompt holds the text the user entered. The input buffer still holds
 *       the same text.
 *
 * @note getch() blocks until a key is pressed. Non-printable keys, such as
 *       arrow keys, are ignored. An ERR return is also ignored, but only
 *       because it fails the isprint() test. In non-blocking mode, this loop
 *       would busy-wait.
 * @note The input buffer is modified without holding ui_mutex. The render thread
 *       can read it at the same time, so this is a data race unless the caller
 *       guarantees that the render thread is not reading the buffer.
 */
bool UserInterface::next_prompt(std::string* prompt) {
    bool done = false;

    this->request_redraw();

    this->backend->clear_input_buffer();

    timeout(100); // getch() returns ERR after 100 ms if no key is pressed
    int ch;

    while ((ch = getch()) != '\n') {
        if (ch == KEY_BACKSPACE || ch == 127) {
            if (!this->backend->input_buffer_is_empty()) {
                this->backend->pop_back_input_buffer();
                this->backend->shift_input_cursor_x(-1);
                this->request_redraw();
            }
        } else if (ch != ERR && isprint(ch)) {
            if (this->backend->room_for_prompt_in_input_buffer()) {
                this->backend->push_back_input_buffer(ch);
                this->backend->shift_input_cursor_x(1);
                this->request_redraw();
            }
        } else if (ch == ERR && !this->backend->is_running()) {
            return true;
        }
    }

    *prompt = this->backend->get_input_buffer();

    if (*prompt == "q" || *prompt == "Q") {
        done = true;
    }

    return done;
}

/**
 * @brief Captures the next user prompt via stdin in non-interactive mode and detects quit commands.
 *
 * @param prompt A pointer to a string where the captured prompt will be stored.
 *
 * @return bool True if the user entered "q" or "Q" (quit command); false otherwise.
 *
 * Prompts the user via stdout and reads a line from stdin using std::getline().
 * Terminates the program with exit code 1 if input reading fails.
 * Detects quit commands ("q" or "Q") and returns true to signal session termination.
 *
 * @note Must be used outside an active ncurses session. Uses stdout/stderr for I/O.
 * @see next_prompt()
 */
bool UserInterface::next_prompt_noninteractive(std::string* prompt) {
    bool done = false;
    printf("⏳ Type your prompt (q/Q to quit): ");
    if (!std::getline(std::cin, *prompt, '\n')) {
        fprintf(stderr, "Error reading user input.\n");
        if (auto ui = instance.lock()) {
            ui->end_session(EXIT_FAILURE);
        }
    }

    if (*prompt == "q" || *prompt == "Q") {
        done = true;
    }

    return done;
}

/**
 * @brief Queues an error message (bold red with ❌ prefix) and requests a redraw.
 *
 * @param text The error message content.
 */
void UserInterface::print_error(const std::string& text) {
    this->backend->print(text, TerminalBackend::MESSAGE_TYPE_ERROR);
}

/**
 * @brief Queues a success message (bold with ✅ prefix) and requests a redraw.
 *
 * @param text The success message content.
 */
void UserInterface::print_success(const std::string& text) {
    this->backend->print(text, TerminalBackend::MESSAGE_TYPE_SUCCESS);
}

/**
 * @brief Queues a normal message (plain text) and requests a redraw.
 *
 * @param text The message content.
 * @param stream In the stream mode, every chunk of text after the first one is appended to the last buffer entry.
 * Defaults to 'false'.
 */
void UserInterface::print_message(const std::string& text, bool stream) {
    this->backend->print(text, TerminalBackend::MESSAGE_TYPE_NORMAL, stream);
}

/**
 * @brief Outputs a message directly to stderr in non-interactive mode.
 *
 * @param text The message content to be displayed.
 *
 * Writes the text to stderr and immediately flushes the output buffer to ensure
 * the message is visible without waiting for a newline.
 * Adds an icon ❌ before the text.
 *
 * @note Must be used outside an active ncurses session.
 * @see print_error()
 */
void UserInterface::print_error_noninteractive(const std::string& text) {
    fprintf(stderr, "❌ %s", text.c_str());
    fflush(stderr);
}

/**
 * @brief Outputs a message directly to stdout in non-interactive mode.
 *
 * @param text The message content to be displayed.
 *
 * Writes the text to stdout and immediately flushes the output buffer to ensure
 * the message is visible without waiting for a newline.
 * Adds an icon ✅ before the text.
 *
 * @note Must be used outside an active ncurses session.
 * @see print_error()
 */
void UserInterface::print_success_noninteractive(const std::string& text) {
    fprintf(stdout, "✅ %s", text.c_str());
    fflush(stdout);
}

/**
 * @brief Outputs a message directly to stdout in non-interactive mode.
 *
 * @param text The message content to be displayed.
 *
 * Writes the text to stdout and immediately flushes the output buffer to ensure
 * the message is visible without waiting for a newline.
 *
 * @note Must be used outside an active ncurses session.
 * @see print_message()
 */
void UserInterface::print_message_noninteractive(const std::string& text) {
    fprintf(stdout, "%s", text.c_str());
    fflush(stdout);
}

/**
 * Requests a redraw and wakes a thread waiting on cv_redraw.
 *
 * The function sets should_redraw to true through set_should_redraw(true), then
 * calls notify_cv_redraw() with its default argument. The method does not
 * acquire ui_mutex.
 *
 * @pre backend is not null.
 * @post should_redraw is true, and a waiter on cv_redraw has been notified.
 *
 * @note The flag is set before the notification, and there is no lock between
 *       them. A waiting thread can wake, see the flag, and proceed before the
 *       notification is sent. The waiter can then miss the signal and go back
 *       to sleep, or it can process the flag without needing the notification.
 * @warning The waiter's predicate must be checked under ui_mutex. Otherwise a
 *          notification sent between the waiter's check and its wait can be
 *          lost. Whether one or all waiters are woken depends on the default
 *          argument of notify_cv_redraw(), which should be documented at that
 *          declaration.
 */
void UserInterface::request_redraw(void) {
    this->backend->set_should_redraw(true);
    this->backend->notify_cv_redraw();
}

/**
 * Starts the spinner from frame 0 and requests a redraw.
 *
 * The function sets spinner_active to true, resets the spinner frame to 0
 * through set_spinner_frame(0), and sets should_redraw to true. Each setter is
 * atomic on its own, but the three updates are not performed as a group and
 * ui_mutex is not held. cv_redraw is not notified.
 *
 * @pre backend is not null.
 * @post get_spinner_active() returns true, the spinner frame is 0, and
 *       should_redraw is true.
 *
 * @note A render thread can read the spinner state between the updates and see
 *       a partial result, such as an active spinner that still shows the
 *       previous frame.
 * @warning If the three updates must appear together, hold ui_mutex across all
 *          of them with a single scoped lock. Call notify_cv_redraw() afterward
 *          if a waiting thread needs to wake up.
 */
void UserInterface::start_spinner() {
    this->backend->set_spinner_active(true);
    this->backend->set_spinner_frame(0);
    this->backend->set_should_redraw(true);
}

/**
 * Stops the spinner, resets its frame to 0, and requests a redraw.
 *
 * The function sets spinner_active to false, resets the spinner frame to 0
 * through set_spinner_frame(0), and sets should_redraw to true. Each setter is
 * atomic on its own, but the three updates are not performed as a group and
 * ui_mutex is not held. cv_redraw is not notified.
 *
 * @pre backend is not null.
 * @post get_spinner_active() returns false, the spinner frame is 0, and
 *       should_redraw is true.
 *
 * @note A render thread can read the spinner state between the updates and see
 *       a partial result, such as an inactive spinner with the previous frame
 *       still set.
 * @warning If the three updates must appear together, hold ui_mutex across all
 *          of them with a single scoped lock. Call notify_cv_redraw() afterward
 *          if a waiting thread needs to wake up.
 */
void UserInterface::stop_spinner() {
    this->backend->set_spinner_active(false);
    this->backend->set_spinner_frame(0);
    this->backend->set_should_redraw(true);
}

/**
 * Advances the spinner to its next frame and requests a redraw, if the spinner
 * is active.
 *
 * The function first checks get_spinner_active(). If the spinner is running, it
 * advances the frame through set_spinner_frame() and then sets should_redraw.
 * Nothing happens if the spinner is inactive. The check and the update are not
 * performed under a lock, and the method does not notify cv_redraw.
 *
 * @pre backend is not null.
 * @post If the spinner was active at the check, its frame has advanced by one
 *       step and should_redraw is true.
 *
 * @note The check and the update are not atomic together. If the spinner is
 *       deactivated between them, the frame can still advance once after
 *       deactivation. This is usually harmless for a visual effect.
 * @warning Because cv_redraw is not notified, a waiting thread may not redraw
 *          until it is woken by another call. Use notify_cv_redraw() if an
 *          immediate redraw is needed.
 */
void UserInterface::advance_spinner_frame() {
    if (this->backend->get_spinner_active()) {
        this->backend->set_spinner_frame();
        this->backend->set_should_redraw(true);
    }
}

/**
 * Updates the displayed progress percentage and requests a redraw.
 *
 * The value is clamped to the range [0, 100] before being stored in the backend.
 * The progress value and the redraw flag are set through two separate atomic
 * operations, so the change is not applied as a single unit. This method does
 * not acquire ui_mutex and does not notify cv_redraw.
 *
 * @param percent The requested progress percentage. Values below 0 are stored
 *                as 0, and values above 100 are stored as 100.
 *
 * @pre backend is not null.
 * @post The backend's progress value is between 0 and 100, inclusive.
 * @post should_redraw is set to true.
 *
 * @note A thread that reads the progress value between the two calls can see
 *       the new percentage before the redraw request is set. This is usually
 *       harmless but can cause a brief inconsistency.
 * @warning Because cv_redraw is not notified, a thread waiting on it may not
 *          wake up to redraw. Callers that need an immediate redraw must notify
 *          it separately with notify_cv_redraw().
 */
void UserInterface::set_progress(int percent) {
    this->backend->set_progress_percent(std::clamp(percent, 0, 100));
    this->backend->set_should_redraw(true);
}
