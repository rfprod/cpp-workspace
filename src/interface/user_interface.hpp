#pragma once

#include "utils/text_utils.hpp"
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
 * @brief UI layout positioning configuration.
 *
 * Stores calculated row and column positions for all major UI elements
 * relative to terminal dimensions. Updated by update_layout() when terminal
 * is resized.
 */
struct Layout {
    // Row for input prompt.
    int input_y;
    // Column for input start.
    int input_x;
    // Row where output begins.
    int output_y;
    // Output area height.
    int output_height;
    // Row for status messages.
    int status_y;
    // Row for the progress spinner.
    int spinner_y;
    // Column for the progress spinner.
    int spinner_x;
};

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
    // Message configuration constants.

    // Plain text, default styling.
    static constexpr int MESSAGE_TYPE_NORMAL = 0;
    // Bold red with ❌ prefix.
    static constexpr int MESSAGE_TYPE_ERROR = 1;
    // Bold with ✅ prefix.
    static constexpr int MESSAGE_TYPE_SUCCESS = 2;

    // User Interface state.

    volatile sig_atomic_t resize_flag;
    volatile sig_atomic_t running;
    // A pointer to the user interface instance.
    static UserInterface* instance;

    // Layout state.
    struct Layout layout;

    // Input buffer state.
    std::string input_buffer;
    int input_cursor_x;

    // Output buffer state.
    std::vector<std::pair<std::string, int>> output_buffer;

    // Renderer state.
    std::thread render_thread;
    std::mutex ui_mutex;
    std::condition_variable cv_redraw;
    std::atomic<bool> should_redraw{true};
    std::atomic<bool> thread_running{false};

    // Spinner state.
    std::atomic<bool> spinner_active{false};
    std::atomic<int> spinner_frame{0};
    std::atomic<int> progress_percent{0};

    // Color theme configuration.
    bool use_dark_theme = true;

    // Initialization methods.

    void init_dark_theme(void);

    void init_light_theme(void);

    void init_ncurses(void);

    void cleanup_ncurses(void);

    // Layout manpulation methods.

    void draw_ui(void);

    void draw_spinner(void);

    void update_layout();

    // Input area methods.

    void clear_input_area();

    void draw_input_area();

    // Output area methods.

    void draw_output_area();

    // Renderer methods.

    void start_render_thread(void);

    void stop_render_thread(void);

    void render_loop(void);

  public:
    UserInterface();

    explicit UserInterface(bool dark_theme);

    ~UserInterface();

    bool is_running();

    static void handle_resize(int sig);

    static void handle_interrupt(int sig);

    void run(void);

    void end_session(int exit_code);

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

    void print(const std::string& text, int type = UserInterface::MESSAGE_TYPE_NORMAL, bool stream = false);

    void print_error(const std::string& text);

    static void print_error_noninteractive(const std::string& text);

    void print_success(const std::string& text);

    static void print_success_noninteractive(const std::string& text);

    void print_message(const std::string& text, bool stream = false);

    static void print_message_noninteractive(const std::string& text);

    // Rendered methods

    void request_redraw(void);

    // Spinner methods.

    void start_spinner(void);

    void stop_spinner(void);

    void advance_spinner_frame(void);

    bool is_spinner_active(void) const;

    void set_progress(int percent);

    // Grant friend access to ProgressSpinner
    friend class ProgressSpinner;
};
