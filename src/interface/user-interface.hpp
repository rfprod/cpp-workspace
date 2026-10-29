#pragma once

#include "utils/text-utils.hpp"
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

    // Color theme configuration.
    bool use_dark_theme = true;

    // Initialization methods.

    /**
     * Initializes the color pairs for the UI dark theme.
     */
    void init_dark_theme(void);

    /**
     * Initializes the color pairs for the UI light theme.
     */
    void init_light_theme(void);

    /**
     * Initializes the ncurses library for terminal UI rendering.
     * Sets up color pairs, configures terminal mode (raw mode, no echo),
     * hides cursor, enables keypad input, and initializes screen dimensions.
     */
    void init_ncurses(void);

    /**
     * Cleans up and restores the terminal to its original state.
     * Disables raw mode, re-enables echo, restores cursor visibility,
     * and deallocates ncurses resources.
     */
    void cleanup_ncurses(void);

    // Renderer methods.

    /**
     * Spawns a dedicated background thread that runs render_loop().
     * The thread handles periodic UI redraws without blocking input handling.
     */
    void start_render_thread(void);

    /**
     * Signals the render thread to stop and waits for it to join.
     * Sets thread_running to false and notifies the condition variable.
     */
    void stop_render_thread(void);

    /**
     * Main loop executed by the render thread.
     * Waits for redraw requests (via condition_variable), then redraws
     * the entire UI if the terminal wasn't resized. Exits when thread_running is false.
     * Thread-safe: uses ui_mutex to protect rendering operations.
     */
    void render_loop(void);

    // Layout manpulation methods.

    /**
     * Redraws the entire user interface.
     * Clears the screen and calls draw_input_area(), draw_output_area(),
     * draw_spinner(), and refreshes the display.
     */
    void draw_ui(void);

    /**
     * Draws the progress spinner at the designated position.
     * Displays an animated character based on the current spinner frame.
     */
    void draw_spinner(void);

    /**
     * Recalculates layout positions and dimensions based on current terminal size.
     * Called when SIGWINCH (terminal resize) is detected.
     * Updates the Layout struct fields (input_y, output_y, status_y, etc.).
     */
    void update_layout();

    // Input area methods.

    /**
     * Renders the input prompt and user input buffer.
     * Displays the prompt text and the current contents of input_buffer,
     * with cursor positioned at input_cursor_x.
     */
    void draw_input_area();

    /**
     * Clears the input area of the screen.
     * Used before redrawing to prevent display artifacts.
     */
    void clear_input_area();

    // Output area methods.

    /**
     * Renders all messages in the output buffer.
     * Iterates through output_buffer and draws each message with its
     * associated type (normal, error, success) which may affect coloring.
     */
    void draw_output_area();

  public:
    /**
     * Constructor: Initializes the UserInterface object.
     * Sets up initial state, layout, and registers signal handlers
     * but does NOT start the render thread (that happens in run()).
     */
    UserInterface();
    /**
     * Constructor: Initializes the UserInterface object.
     * Sets up initial state, layout, and registers signal handlers
     * but does NOT start the render thread (that happens in run()).
     * @param dark_theme Whether to active the dark theme.
     */
    explicit UserInterface(bool dark_theme);

    /**
     * Destructor: Cleans up resources.
     * Stops the render thread and restores the terminal via cleanup_ncurses().
     */
    ~UserInterface();

    /**
     * Returns whether the UI is currently running.
     * Reads the volatile 'running' flag in a thread-safe manner.
     * @return true if the UI is running, false otherwise.
     */
    bool is_running();

    /**
     * Static signal handler for SIGWINCH (terminal resize).
     * Sets resize_flag to 1 to trigger layout recalculation.
     * Must be static to comply with C signal API.
     * @param sig The signal number (unused, present for signal handler compliance).
     */
    static void handle_resize(int sig);

    /**
     * Static signal handler for SIGINT (Ctrl+C).
     * Sets running to 0 to gracefully shut down the UI.
     * Must be static to comply with C signal API.
     * @param sig The signal number (unused, present for signal handler compliance).
     */
    static void handle_interrupt(int sig);

    /**
     * Starts the interactive user interface.
     * Registers signal handlers, initializes ncurses, starts the render thread,
     * and enters the main input loop. Blocks until running becomes false.
     */
    void run(void);

    /**
     * Ends the current session and exits.
     * Stops the render thread, cleans up ncurses, and terminates the application.
     * @param exit_code The program exit code to return to the operating system.
     */
    void end_session(int exit_code);

    /**
     * Displays configuration/arguments at the start of the session (interactive mode).
     * Outputs model, prompt, and mode information to the UI.
     * @param model The model being used.
     * @param prompt The initial prompt text.
     * @param stream Whether streaming is enabled.
     * @param interactive Whether interactive mode is enabled.
     * @param dark_theme Whether to active the dark theme.
     */
    void print_args(const std::string& model, const std::string& prompt, bool stream, bool interactive,
                    bool dark_theme);

    /**
     * Static version of print_args() for non-interactive mode.
     * Outputs arguments to stdout instead of the UI.
     * @param model The model being used.
     * @param prompt The initial prompt text.
     * @param stream Whether streaming is enabled.
     * @param interactive Whether interactive mode is enabled.
     * @param dark_theme Whether to active the dark theme.
     */
    static void print_args_noninteractive(const std::string& model, const std::string& prompt, bool stream,
                                          bool interactive, bool dark_theme);

    /**
     * Prints available models and their corresponding indexes for selection.
     * @param models The list of available local models.
     */
    void print_model_list(const std::vector<std::string>& models);

    /**
     * Static version of print_model_list() for non-interactive mode.
     * @param models The list of available local models.
     */
    static void print_model_list_noninteractive(const std::vector<std::string>& models);

    /**
     * Prompts for user input to select a model form the list of available local models.
     * @param models The list of available local models.
     * @param model Pointer to a std::string to receive the user input.
     */
    void select_model(const std::vector<std::string>& models, std::string* model);

    /**
     * Static version of the select_model() for non-interactive mode.
     * @param models The list of available local models.
     * @param model Pointer to a std::string to receive the user input.
     */
    static void select_model_noninteractive(const std::vector<std::string>& models, std::string* model);

    /**
     * Waits for and retrieves the next user input prompt (interactive mode).
     * Blocks until the user presses Enter and a prompt is available.
     * @param prompt Pointer to a std::string to receive the user input.
     * @return true if a prompt was successfully read, false if the session ended.
     */
    bool next_prompt(std::string* prompt);

    /**
     * Static version of next_prompt() for non-interactive mode.
     * Reads from stdin instead of the interactive UI.
     * @param prompt Pointer to a std::string to receive the user input.
     * @return true if input was read, false on EOF or error.
     */
    static bool next_prompt_noninteractive(std::string* prompt);

    /**
     * Prints a message to the output area with a specified type.
     * Appends the message to output_buffer and triggers a redraw.
     * @param text The message to display.
     * @param type The message type (MESSAGE_TYPE_NORMAL, MESSAGE_TYPE_ERROR, or
     * MESSAGE_TYPE_SUCCESS). Defaults to MESSAGE_TYPE_NORMAL if not specified.
     */
    void print(const std::string& text, int type = UserInterface::MESSAGE_TYPE_NORMAL);

    /**
     * Convenience method: prints an error message (MESSAGE_TYPE_ERROR).
     * @param text The error message to display.
     */
    void print_error(const std::string& text);

    /**
     * Convenience method: prints a success message (MESSAGE_TYPE_SUCCESS).
     * @param text The success message to display.
     */
    void print_success(const std::string& text);

    /**
     * Convenience method: prints a normal message (MESSAGE_TYPE_NORMAL).
     * @param text The message to display.
     */
    void print_message(const std::string& text);

    void print_message_noninteractive(const std::string& text);

    /**
     * Requests that the UI be redrawn on the next render cycle.
     * Sets should_redraw to true and notifies the render thread via condition variable.
     * Safe to call from multiple threads.
     */
    void request_redraw(void);

    // Spinner methods.

    /**
     * Activates the progress spinner animation.
     * Sets spinner_active to true.
     */
    void start_spinner(void);

    /**
     * Deactivates the progress spinner.
     * Sets spinner_active to false.
     */
    void stop_spinner(void);

    /**
     * Advances the spinner animation to the next frame.
     * Increments spinner_frame (wraps around after reaching max).
     * Called periodically during render_loop().
     */
    void advance_spinner_frame(void);

    /**
     * Returns the current state of the spinner.
     * @return true if the spinner is currently active, false otherwise.
     */
    bool is_spinner_active(void) const;

    /**
     * Sets the progress percentage displayed with the spinner.
     * @param percent Progress as a percentage (0–100).
     */
    void set_progress(int percent);

    // Grant friend access to ProgressSpinner
    friend class ProgressSpinner;
};
