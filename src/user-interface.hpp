#pragma once

#include "text-utils.hpp"
#include <atomic>
#include <condition_variable>
#include <functional>
#include <mutex>
#include <ncurses.h>
#include <signal.h>
#include <string>
#include <thread>
#include <vector>

struct Layout {
    int input_y;  // Row for input prompt.
    int input_x;  // Column for input start.
    int output_y; // Row where output begins.
    int output_height;
    int status_y;  // Row for status messages.
    int spinner_y; // Row for the progress spinner.
    int spinner_x; // Column for the progress spinner.
};

class UserInterface {
  private:
    // Message configuration constants.
    static constexpr int MESSAGE_TYPE_NORMAL = 0;
    static constexpr int MESSAGE_TYPE_ERROR = 1;
    static constexpr int MESSAGE_TYPE_SUCCESS = 2;

    // User Interface state.
    volatile sig_atomic_t resize_flag;
    volatile sig_atomic_t running;
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

    // Initialization methods.
    void init_ncurses(void);
    void cleanup_ncurses(void);

    // Renderer methods.
    void start_render_thread(void);
    void stop_render_thread(void);
    void render_loop(void);

    // Layout manpulation methods.
    void draw_ui(void);
    void draw_spinner(void);
    void update_layout();

    // Input area methods.
    void draw_input_area();
    void clear_input_area();

    // Output area methods.
    void draw_output_area();

  public:
    UserInterface();
    ~UserInterface();

    bool is_running();

    static void handle_resize(int sig);
    static void handle_interrupt(int sig);

    void run(void);
    void end_session(int exit_code);

    void print_args(const std::string& model, const std::string& prompt, bool stream,
                    bool interactive);
    static void print_args_noninteractive(const std::string& model, const std::string& prompt,
                                          bool stream, bool interactive);

    bool next_prompt(std::string* prompt);
    static bool next_prompt_noninteractive(std::string* prompt);

    void print(const std::string& text, int type = UserInterface::MESSAGE_TYPE_NORMAL);
    void print_error(const std::string& text);
    void print_success(const std::string& text);
    void print_message(const std::string& text);

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
