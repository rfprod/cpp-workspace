#pragma once

#include "terminal_backend.hpp"
#include <atomic>
#include <condition_variable>
#include <functional>
#include <mutex>
#include <ncurses.h>
#include <signal.h>
#include <string>
#include <thread>
#include <vector>

/**
 * @brief ncurses-based terminal user interface for interactive prompting and output display.
 *
 * Manages full-screen UI rendering with UTF-8 box borders, input buffer, scrollable output,
 * animated progress spinner, and configurable dark/light themes. Runs a background render
 * thread that updates the display on demand or every 1500ms. Handles terminal resize
 * (SIGWINCH) and interrupt (SIGINT) signals. Supports dual interactive/noninteractive modes.
 *
 * Message types: MESSAGE_TYPE_NORMAL (plain text), MESSAGE_TYPE_ERROR (bold red with ❌),
 * MESSAGE_TYPE_SUCCESS (bold with ✅).
 *
 * @note Singleton pattern via static instance pointer; used by signal handlers.
 * @note Thread-safe: output_buffer and spinner state protected by ui_mutex and atomic flags.
 * @note Must call run() to initialize ncurses and start rendering before using print/spinner methods.
 * @note Noninteractive methods (print_*_noninteractive, select_model_noninteractive, next_prompt_noninteractive)
 *       must be used outside an active ncurses session.
 * @see run(), end_session(), ProgressSpinner (friend class)
 */
class UserInterface {
  private:
    // A pointer to the user interface instance.
    static std::weak_ptr<UserInterface> instance;

    // A pointer to the terminal backend instance.
    std::unique_ptr<TerminalBackend> backend;

  public:
    UserInterface();

    explicit UserInterface(bool dark_theme);

    ~UserInterface();

    static std::shared_ptr<UserInterface> create();

    static std::shared_ptr<UserInterface> create(bool dark_theme);

    bool backend_running();

    void run(void);

    void end_session(int code = EXIT_SUCCESS);

    void print_args(const std::string& model, const std::string& embedding_model, const std::string& prompt,
                    bool stream, bool rag, bool interactive, bool dark_theme, bool skip_embedding);

    static void print_args_noninteractive(const std::string& model, const std::string& embedding_model,
                                          const std::string& prompt, bool stream, bool rag, bool interactive,
                                          bool dark_theme, bool skip_embedding);

    void print_model_list(const std::vector<std::string>& models);

    static void print_model_list_noninteractive(const std::vector<std::string>& models);

    void select_model(const std::vector<std::string>& models, std::string* model);

    static void select_model_noninteractive(const std::vector<std::string>& models, std::string* model);

    bool next_prompt(std::string* prompt);

    static bool next_prompt_noninteractive(std::string* prompt);

    void print_error(const std::string& text);

    static void print_error_noninteractive(const std::string& text);

    void print_success(const std::string& text);

    static void print_success_noninteractive(const std::string& text);

    void print_message(const std::string& text, bool stream = false);

    static void print_message_noninteractive(const std::string& text);

    // Renderer methods

    void request_redraw(void);

    // Spinner methods.

    void start_spinner(void);

    void stop_spinner(void);

    void advance_spinner_frame(void);

    void set_progress(int percent);

    // Grant friend access to ProgressSpinner
    friend class ProgressSpinner;
};
