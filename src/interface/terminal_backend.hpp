#pragma once

#include "terminal_backend.hpp"
#include "utils/text_utils.hpp"
#include <atomic>
#include <condition_variable>
#include <exception>
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

class TerminalBackend {
  private:
    // Terminal backend state.
    static std::atomic<bool> resize_flag;
    static std::atomic<bool> interrupt_flag;

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
    std::mutex exception_mutex;
    std::condition_variable cv_redraw;
    std::atomic<bool> should_redraw{true};
    std::atomic<bool> thread_running{false};
    std::atomic<bool> terminal_restored{false};

    // Spinner state.
    std::atomic<bool> spinner_active{false};
    std::atomic<int> spinner_frame{0};
    std::atomic<int> progress_percent{0};

    std::exception_ptr render_exception;

    // Color theme configuration.
    bool use_dark_theme{false};

    // Initialization methods.

    void init_dark_theme(void);

    void init_light_theme(void);

    void init_ncurses(void);

    // Signal handlers.

    void install_signal_handlers();
    static void handle_resize(int sig);
    static void handle_interrupt(int sig);

  public:
    TerminalBackend();

    explicit TerminalBackend(bool dark_theme);

    ~TerminalBackend();

    void end_session(int code = EXIT_SUCCESS);

    void run(void);

    bool is_running();

    // Message configuration constants.

    // Plain text, default styling.
    static constexpr int MESSAGE_TYPE_NORMAL = 0;
    // Bold red with ❌ prefix.
    static constexpr int MESSAGE_TYPE_ERROR = 1;
    // Bold with ✅ prefix.
    static constexpr int MESSAGE_TYPE_SUCCESS = 2;

    // Value setters
    void shift_input_cursor_x(int shift);
    void set_should_redraw(bool value);
    void set_spinner_active(bool value);
    void set_spinner_frame(int value = -1);
    void set_progress_percent(int value);

    void notify_cv_redraw(bool all = false);

    // Value getters.
    bool get_spinner_active(void);

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

    // Output printing methods.

    void print(const std::string& text, int type, bool stream = false);

    // Input buffer access methods.
    void clear_input_buffer(void);
    bool exit_keys_in_input_buffer(void);
    bool room_for_prompt_in_input_buffer(void);
    bool input_buffer_is_empty(void);
    std::string_view get_input_buffer(void) const;
    void pop_back_input_buffer(void);
    void push_back_input_buffer(int ch);
};
