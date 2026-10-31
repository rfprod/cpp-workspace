#include "user_interface.hpp"
#include <chrono>
#include <csignal>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <locale>
#include <ncurses.h>
#include <sstream>

UserInterface* UserInterface::instance = nullptr;

/**
 * Semantic color pair identifiers for terminal UI rendering.
 *
 * Defines named color pair constants for consistent theming across the application.
 * Each constant maps to an ncurses color pair ID, enabling semantic color assignment
 * for different UI contexts and message types. Values range from 1 to 9, matching
 * ncurses color pair initialization indices.
 *
 * - DEFAULT_PAIR: Standard text color for neutral content
 * - HIGHLIGHT_PAIR: Emphasis color for selected or important items
 * - SUCCESS_PAIR: Color for successful operations or positive messages
 * - WARNING_PAIR: Color for cautionary messages or non-critical issues
 * - ERROR_PAIR: Color for error messages and critical failures
 * - INFO_PAIR: Color for informational messages
 * - MUTED_PAIR: Subdued color for de-emphasized content
 * - ACCENT_PAIR: Decorative color for visual variety and UI separation
 * - FOCUS_PAIR: Color for focused or active UI elements
 */
enum ColorPair {
    DEFAULT_PAIR = 1,
    HIGHLIGHT_PAIR = 2,
    SUCCESS_PAIR = 3,
    WARNING_PAIR = 4,
    ERROR_PAIR = 5,
    INFO_PAIR = 6,
    MUTED_PAIR = 7,
    ACCENT_PAIR = 8,
    FOCUS_PAIR = 9
};

/**
 * Initializes ncurses color pairs with a dark theme color scheme.
 *
 * Enables ncurses color support via start_color(), allows the terminal's default
 * background color to be used via use_default_colors(), and defines all ColorPair
 * color pair constants with semantic foreground/background combinations.
 *
 * Most pairs use -1 as background, preserving the terminal's default background
 * color. HIGHLIGHT_PAIR uses black text on white background for strong contrast,
 * and FOCUS_PAIR uses white text on blue background for focused element visibility.
 *
 * This method should be called early during UserInterface initialization, before
 * any colored text is rendered to the terminal.
 */
void UserInterface::init_dark_theme(void) {
    start_color();
    use_default_colors();

    // Purpose | Foreground | Background
    init_pair(DEFAULT_PAIR, COLOR_WHITE, -1);
    init_pair(HIGHLIGHT_PAIR, COLOR_BLACK, COLOR_WHITE);
    init_pair(SUCCESS_PAIR, COLOR_GREEN, -1);
    init_pair(WARNING_PAIR, COLOR_YELLOW, -1);
    init_pair(ERROR_PAIR, COLOR_RED, -1);
    init_pair(INFO_PAIR, COLOR_CYAN, -1);
    init_pair(ACCENT_PAIR, COLOR_MAGENTA, -1);
    init_pair(FOCUS_PAIR, COLOR_WHITE, COLOR_BLUE);
}

/**
 * Initializes ncurses color pairs with a light theme color scheme.
 *
 * Enables ncurses color support via start_color(), allows the terminal's default
 * background color to be used via use_default_colors(), and defines all ColorPair
 * color pair constants with semantic foreground/background combinations optimized
 * for light terminal backgrounds.
 *
 * Unlike the dark theme, most pairs use white backgrounds for contrast against
 * darker text. DEFAULT_PAIR uses black text on white for standard content, and
 * HIGHLIGHT_PAIR inverts to white text on black for strong emphasis. FOCUS_PAIR
 * uses black text on cyan background for focused element visibility.
 *
 * This method should be called early during UserInterface initialization, before
 * any colored text is rendered to the terminal. Select init_dark_theme() or
 * init_light_theme() based on the user's terminal background preference.
 */
void UserInterface::init_light_theme(void) {
    start_color();
    use_default_colors();

    // Purpose | Foreground | Background
    init_pair(DEFAULT_PAIR, COLOR_BLACK, COLOR_WHITE);
    init_pair(HIGHLIGHT_PAIR, COLOR_WHITE, COLOR_BLACK);
    init_pair(SUCCESS_PAIR, COLOR_GREEN, COLOR_WHITE);
    init_pair(WARNING_PAIR, COLOR_YELLOW, COLOR_WHITE);
    init_pair(ERROR_PAIR, COLOR_RED, COLOR_WHITE);
    init_pair(INFO_PAIR, COLOR_BLUE, COLOR_WHITE);
    init_pair(ACCENT_PAIR, COLOR_MAGENTA, COLOR_WHITE);
    init_pair(FOCUS_PAIR, COLOR_BLACK, COLOR_CYAN);
}

/**
 * Initializes the ncurses terminal UI with locale support, terminal configuration, color theming, and signal handling.
 *
 * Sets the locale to UTF-8 for character encoding support and establishes TERMINFO fallback to the system
 * terminfo database if not already set (handles Conan ncurses builds). Initializes ncurses with initscr(),
 * terminating with error diagnostics on failure. Updates window layout via update_layout().
 *
 * Configures terminal I/O: enables cbreak mode (immediate input) and disables echo. Attempts to enable
 * function key and arrow key support via keypad(); non-fatal warning if unsupported. Sets input blocking
 * mode (getch() waits for user input). Attempts to hide the cursor; non-fatal warning if unsupported.
 *
 * If the terminal supports colors, initializes color pairs by calling init_dark_theme() or init_light_theme()
 * based on the use_dark_theme flag, and sets the terminal background to DEFAULT_PAIR. Registers SIGWINCH
 * signal handler for terminal window resize detection and SIGINT handler for interrupt cleanup.
 *
 * Terminates the program on critical failures (initscr failure or terminal setup failure).
 * Non-critical feature failures (keypad, cursor hiding) produce warnings but do not halt initialization.
 */
void UserInterface::init_ncurses(void) {
    // Set up locale for UTF-8 support.
    setlocale(LC_ALL, "");

    // Set TERMINFO to system database if Conan ncurses can't find it.
    if (!std::getenv("TERMINFO")) {
        setenv("TERMINFO", "/usr/share/terminfo", 0);
    }

    // Initialize ncurses.
    const WINDOW* w = initscr();
    if (!w) {
        const char* term = getenv("TERM");
        std::cerr << "Failed to initialize ncurses with TERM=" << (term ? term : "unset") << "\n";
        std::exit(EXIT_FAILURE);
    }

    this->update_layout();

    // Setup terminal.
    if (cbreak() == ERR || noecho() == ERR) {
        endwin();
        std::cerr << "Terminal setup failed\n";
        std::exit(EXIT_FAILURE);
    }

    // Enable function keys and arrow keys.
    if (keypad(stdscr, TRUE) == ERR) {
        std::cerr << "Warning: keypad() not supported\n";
    }

    // Block on input (getch waits for input).
    nodelay(stdscr, FALSE);

    // Cursor hiding (may not be supported).
    if (curs_set(0) == ERR) {
        std::cerr << "Warning: cursor control not supported on this terminal\n";
    }

    if (has_colors()) {
#ifdef DEBUG
        if (this->is_running() == false) {
            UserInterface::print_message_noninteractive(
                std::format("\t[DEBUG] Checking theme - use_dark_theme = {}\n", this->use_dark_theme));
        }
#endif

        if (this->use_dark_theme == true) {
            this->init_dark_theme();
        } else {
            this->init_light_theme();
        }
        bkgd(COLOR_PAIR(DEFAULT_PAIR));
    }

    signal(SIGWINCH, UserInterface::handle_resize);
    signal(SIGINT, UserInterface::handle_interrupt);
}

/**
 * Cleans up ncurses and restores the terminal to its normal state.
 *
 * Calls endwin() to tear down the ncurses library, which restores terminal settings (echo, cbreak mode,
 * cursor visibility, color state) and clears the screen. Must be called before program termination to
 * prevent terminal corruption.
 */
void UserInterface::cleanup_ncurses(void) {
    endwin();
}

/**
 * Draws the terminal UI frame with a Unicode box border.
 *
 * Clears the screen and retrieves the current terminal dimensions (max_y, max_x) via getmaxyx().
 * Draws a complete box frame using Unicode box-drawing characters: ╔ and ╗ (top corners), ╚ and ╝
 * (bottom corners), ═ (horizontal edges), and ║ (vertical edges). Positions characters using mvprintw()
 * to create a full-terminal border with proper corner and edge placement.
 *
 * Refreshes the display with refresh() to flush buffered output and show the drawn frame on screen.
 */
void UserInterface::draw_ui(void) {
    clear();

    int max_y;
    int max_x;
    getmaxyx(stdscr, max_y, max_x);

    int i;

    // Top border
    mvprintw(0, 0, "╔");
    for (i = 1; i < max_x - 1; i += 1) {
        mvprintw(0, i, "═");
    }
    mvprintw(0, max_x - 1, "╗");

    // Side borders
    for (i = 1; i < max_y - 1; i += 1) {
        mvprintw(i, 0, "║");
        mvprintw(i, max_x - 1, "║");
    }

    // Bottom border
    mvprintw(max_y - 1, 0, "╚");
    for (i = 1; i < max_x - 1; i += 1) {
        mvprintw(max_y - 1, i, "═");
    }
    mvprintw(max_y - 1, max_x - 1, "╝");

    refresh();
}

/**
 * Draws an animated spinner with optional progress percentage at the UI layout position.
 *
 * Returns early if the spinner is not active (checked via is_spinner_active()). Loads the current
 * animation frame index and progress percentage from atomic variables (spinner_frame, progress_percent)
 * in a thread-safe manner. Selects the appropriate spinner character from a rotating set (|, /, -, \)
 * based on the frame index.
 *
 * Enables DEFAULT_PAIR color styling and renders the spinner at the layout-defined position
 * (layout.spinner_y, layout.spinner_x) using mvprintw(). If progress > 0, displays "X Loading..."
 * with the progress percentage; otherwise displays "X Loading..." without percentage. Disables color
 * styling after rendering.
 *
 * Thread-safe via atomic variable loads; does not modify shared state.
 */
void UserInterface::draw_spinner() {
    if (!this->is_spinner_active()) {
        return;
    }

    const char spinner_chars[] = {'|', '/', '-', '\\'};
    int frame = this->spinner_frame.load();
    int progress = this->progress_percent.load();

    attron(COLOR_PAIR(DEFAULT_PAIR));

    if (progress > 0) {
        mvprintw(this->layout.spinner_y, this->layout.spinner_x, "%c Progress: %d%%", spinner_chars[frame], progress);
    } else {
        mvprintw(this->layout.spinner_y, this->layout.spinner_x, "%c Loading...", spinner_chars[frame]);
    }

    attroff(COLOR_PAIR(DEFAULT_PAIR));
}

/**
 * Recalculates UI layout positions and dimensions based on current terminal size.
 *
 * Retrieves the current terminal dimensions (max_y, max_x) via getmaxyx() and updates the layout
 * structure to position UI elements relative to terminal boundaries. Sets status_y to the bottom row
 * (max_y - 1), input area to max_y - 3 with left margin of 2 columns, and spinner to max_y - 2
 * (between input and status rows). Sets output area to start at row 1 (below top border) with height
 * calculated as the vertical space between output_y and input_y minus one row for spacing.
 *
 * Intended to be called during initialization and in response to terminal resize events (SIGWINCH)
 * to ensure all UI elements remain properly positioned and sized when the terminal is resized.
 */
void UserInterface::update_layout() {
    int max_y, max_x;
    getmaxyx(stdscr, max_y, max_x);

    layout.status_y = max_y - 1;
    layout.input_y = max_y - 3;
    layout.input_x = 2;
    layout.spinner_y = max_y - 2;
    layout.spinner_x = 2;
    layout.output_y = 1;
    layout.output_height = layout.input_y - layout.output_y - 1;
}

/**
 * Clears the input area rows by overwriting them with spaces.
 *
 * Retrieves the current terminal dimensions (max_y, max_x) via getmaxyx(). Erases two rows of the
 * input area (layout.input_y and layout.input_y + 1) by drawing horizontal lines of spaces from
 * column 1 to max_x - 2, leaving the left and right borders intact.
 *
 * Used to erase previous user input or prompts before rendering new content in the input area.
 * Maintains visual consistency by preserving the terminal border structure.
 */
void UserInterface::clear_input_area() {
    int max_y, max_x;
    getmaxyx(stdscr, max_y, max_x);

    mvhline(layout.input_y, 1, ' ', max_x - 2);
    mvhline(layout.input_y + 1, 1, ' ', max_x - 2);
}

/**
 * Renders the input prompt and user-typed content in the input area.
 *
 * Clears the input area via clear_input_area(). Returns early if the spinner is currently active,
 * preventing prompt and input display while a background operation (query execution) is in progress.
 *
 * When the spinner is inactive, renders a static prompt "⏳ Type your prompt (q/Q to quit): " at
 * the layout-defined input position (layout.input_y, layout.input_x). Renders the contents of the
 * input_buffer (user-typed characters) immediately following the prompt text at a fixed column offset
 * (+37 from input_x).
 *
 * Provides visual feedback and instructions to the user while maintaining a clean, non-overlapping
 * layout during asynchronous spinner operation.
 */
void UserInterface::draw_input_area() {
    this->clear_input_area();
    if (this->is_spinner_active()) {
        return;
    }

    mvprintw(layout.input_y, layout.input_x, "⏳ Type your prompt (q/Q to quit): ");

    // Draw the actual input buffer that user typed
    mvprintw(layout.input_y, layout.input_x + 37, "%s", input_buffer.c_str());
}

/**
 * Renders the message output buffer in reverse chronological order (newest first),
 * from the bottom of the output area upward.
 *
 * Clears the entire output area by filling each row with spaces, preserving the border columns.
 * Iterates backwards through output_buffer, starting from the most recently added message.
 * For each message, extracts the text content and message type (error/success/default).
 *
 * Splits multi-line message text by newline characters. Wraps each logical line at word
 * boundaries using TextUtils::wrap_text() to fit within max_content_width (COLS - 4).
 * Renders wrapped lines from bottom to top (reverse order) within the allocated output area,
 * decrementing current_line after each rendered line.
 *
 * Applies semantic color and icon styling based on message type:
 * - MESSAGE_TYPE_ERROR: bold red (ERROR_PAIR) with ❌ emoji prefix.
 * - MESSAGE_TYPE_SUCCESS: bold default color with ✅ emoji prefix.
 * - Default: plain text without icon.
 *
 * Breaks iteration early if current_line reaches or falls below layout.output_y (top boundary).
 * Calls refresh() to update the terminal display.
 *
 * Provides a scrollable-like view of the most recent messages within the constrained output area,
 * with automatic wrapping and visual differentiation for errors and successful operations.
 */
void UserInterface::draw_output_area() {
    // Clear the output area
    for (int i = 0; i < layout.output_height; i += 1) {
        mvhline(layout.output_y + i, 1, ' ', COLS - 2);
    }

    int current_line = layout.input_y - 2; // Start from bottom
    int max_content_width = COLS - 4;

    // Iterate backwards through buffer (newest first)
    for (auto it = output_buffer.rbegin(); it != output_buffer.rend(); it += 1) {
        if (current_line <= layout.output_y) {
            break;
        }

        const auto& [text, type] = *it;
        std::istringstream stream(text);
        std::string line;
        std::vector<std::string> display_lines;

        // Process each logical line from the message
        while (std::getline(stream, line)) {
            // Wrap long lines at word boundaries
            auto wrapped = TextUtils::wrap_text(line, max_content_width);
            display_lines.insert(display_lines.end(), wrapped.begin(), wrapped.end());
        }

        // Render from bottom to top (reverse order)
        for (auto line_it = display_lines.rbegin(); line_it != display_lines.rend(); line_it += 1) {
            if (current_line <= layout.output_y) break;

            if (type == UserInterface::MESSAGE_TYPE_ERROR) {
                wattron(stdscr, COLOR_PAIR(ERROR_PAIR) | A_BOLD);
                mvprintw(current_line, 2, "❌ %s", line_it->c_str());
                wattroff(stdscr, COLOR_PAIR(ERROR_PAIR) | A_BOLD);
            } else if (type == UserInterface::MESSAGE_TYPE_SUCCESS) {
                wattron(stdscr, A_BOLD);
                mvprintw(current_line, 2, "✅ %s", line_it->c_str());
                wattroff(stdscr, A_BOLD);
            } else {
                mvprintw(current_line, 2, "%s", line_it->c_str());
            }

            current_line--;
        }
    }

    refresh();
}

/**
 * @brief Spawns the background rendering thread and sets the thread_running flag.
 *
 * Sets thread_running to true and creates a new std::thread that invokes render_loop().
 * The thread is joinable and must be cleaned up via stop_render_thread() or equivalent.
 *
 * @note Call this after init_ncurses() to begin continuous UI rendering.
 * @see render_loop(), stop_render_thread()
 */
void UserInterface::start_render_thread(void) {
    this->thread_running = true;
    this->render_thread = std::thread(&UserInterface::render_loop, this);
}

/**
 * @brief Gracefully shuts down the background rendering thread.
 *
 * Sets thread_running to false and notifies the condition variable cv_redraw to
 * wake the render loop. Joins the thread if it is joinable, ensuring complete cleanup.
 *
 * @note Call this during session shutdown or before destroying the UserInterface instance.
 * @see start_render_thread(), render_loop()
 */
void UserInterface::stop_render_thread(void) {
    this->thread_running = false;
    this->cv_redraw.notify_one();
    if (this->render_thread.joinable()) {
        this->render_thread.join();
    }
}

/**
 * @brief Main rendering loop executed by the background render thread.
 *
 * Runs continuously while thread_running is true. Uses cv_redraw condition variable
 * with a 1500ms timeout to wait efficiently for redraw requests. Handles terminal resize
 * events (endwin(), refresh(), update_layout(), cursor recalculation). Conditionally
 * redraws UI components (draw_ui, draw_output_area, draw_input_area, draw_spinner)
 * when should_redraw is set. All ncurses operations are protected by ui_mutex.
 *
 * @note This method is the entry point for the render thread and should not be called directly.
 * Call start_render_thread() to spawn it. Terminate via stop_render_thread().
 * @see start_render_thread(), stop_render_thread(), draw_ui(), draw_output_area(), draw_input_area(), draw_spinner()
 */
void UserInterface::render_loop(void) {
    while (this->thread_running) {
        std::unique_lock<std::mutex> lock(this->ui_mutex);

        // Wait for redraw request or timeout (100ms)
        this->cv_redraw.wait_for(lock, std::chrono::milliseconds(1500), [this] { return this->should_redraw.load(); });

        // Handle resize (under lock)
        if (this->resize_flag) {
            this->resize_flag = 0;
            endwin();
            refresh();
            this->update_layout();

            // Recalculate cursor position after resize
            input_cursor_x = layout.input_x + 37 + input_buffer.length();
            this->should_redraw = true; // Force redraw after resize
        }

        // Redraw UI (all ncurses calls under lock)
        if (this->should_redraw) {
            this->draw_ui();
            this->draw_output_area();
            this->draw_input_area();
            this->draw_spinner();

            refresh();
            this->should_redraw = false;
        }

        lock.unlock();
    }
}

/**
 * Constructs a UserInterface instance and initializes member variables.
 *
 * Initializes atomic flags resize_flag and running to 0 (false/inactive).
 * Initializes input_cursor_x to 0 (cursor at leftmost position).
 * Initializes layout struct to default-constructed values (all fields zeroed/default).
 * Sets use_dark_theme to true, establishing dark theme as the default color scheme preference.
 *
 * Stores a static instance pointer (UserInterface::instance = this) to enable the singleton pattern,
 * allowing static methods and signal handlers to access the active UI instance.
 *
 * Emits debug output to verify use_dark_theme initialization state.
 *
 * Note: This constructor performs only member initialization and singleton registration.
 * Full ncurses initialization (terminal setup, color pair configuration, input handling) is deferred
 * to init_ncurses(), which must be called explicitly after construction to establish the UI environment.
 */
UserInterface::UserInterface() : resize_flag(0), running(0), input_cursor_x(0), layout({}), use_dark_theme(true) {
    UserInterface::instance = this;
}

/**
 * Constructs a UserInterface instance with a caller-specified color theme preference.
 *
 * @param dark_theme: Boolean flag to select color scheme; true for dark theme, false for light theme.
 *
 * Initializes atomic flags resize_flag and running to 0 (false/inactive).
 * Initializes input_cursor_x to 0 (cursor at leftmost position).
 * Initializes layout struct to default-constructed values (all fields zeroed/default).
 * Sets use_dark_theme to the caller-provided dark_theme value, establishing the preferred
 * color scheme that will be applied during init_ncurses().
 *
 * Stores a static instance pointer (UserInterface::instance = this) to enable the singleton pattern,
 * allowing static methods and signal handlers to access the active UI instance.
 *
 * Emits debug output to verify use_dark_theme initialization state.
 *
 * Note: This constructor performs only member initialization and singleton registration.
 * Full ncurses initialization (terminal setup, color pair configuration, input handling) is deferred
 * to init_ncurses(), which must be called explicitly after construction to establish the UI environment.
 */
UserInterface::UserInterface(bool dark_theme)
    : resize_flag(0), running(0), input_cursor_x(0), layout({}), use_dark_theme(dark_theme) {
    UserInterface::instance = this;
}

/**
 * Destructs a UserInterface instance and releases ncurses resources.
 *
 * Calls cleanup_ncurses() to restore terminal settings and release ncurses state.
 * This operation calls endwin() to exit ncurses mode and prevent terminal corruption upon program exit.
 *
 * Nullifies the static instance pointer (UserInterface::instance = nullptr) to clear the singleton reference.
 * This prevents accidental use-after-free from static methods or signal handlers attempting to access
 * a destroyed UI instance.
 *
 * Ensures proper terminal state restoration and safe cleanup of the singleton pattern.
 *
 * Note: Should be called when the UI environment is no longer needed, typically before program termination.
 */
UserInterface::~UserInterface() {
    this->cleanup_ncurses();
    UserInterface::instance = nullptr;
}

/**
 * Queries the current running state of the UI application.
 *
 * @return Boolean indicating whether the application is actively running.
 *
 * - true if running == 1 (active/running);
 *
 * - false if running == 0 (inactive/stopped).
 *
 * Performs a thread-safe atomic read of the running flag, allowing safe concurrent
 * queries from multiple threads without synchronization overhead.
 *
 * Typically used in main event loops or background threads to determine whether
 * to continue processing, poll for input, or exit gracefully.
 */
bool UserInterface::is_running() {
    return this->running == 1;
}

/**
 * Static signal handler for SIGWINCH (terminal resize) events.
 *
 * @param sig Signal number (SIGWINCH); unused but required by signal handler signature.
 *
 * Checks if the singleton UserInterface instance exists. If it does, sets the atomic
 * resize_flag to 1, signaling to the main event loop that a terminal resize has occurred
 * and the layout must be recalculated.
 *
 * Re-registers this handler with signal(SIGWINCH, ...) to ensure continued handling of
 * subsequent resize events. This re-registration is necessary on systems where signal handlers
 * are one-time (non-persistent) and must be re-armed after each invocation.
 *
 * Uses atomic flag-setting rather than directly invoking layout recalculation, deferring
 * resize handling to the main event loop to avoid complex operations within signal context.
 * Provides defensive null-check of the singleton instance in case the handler is invoked
 * after the UI is destroyed.
 *
 * Enables responsive terminal resizing without blocking signal delivery or causing
 * signal-safe function violations.
 */
void UserInterface::handle_resize(int sig) {
    if (UserInterface::instance) {
        UserInterface::instance->resize_flag = 1;
    }

    signal(SIGWINCH, UserInterface::handle_resize);
}

/**
 * Static signal handler for SIGINT (interrupt) signal.
 *
 * @param sig Signal number (SIGINT); unused but required by signal handler signature.
 *
 * Handles graceful shutdown triggered by user interrupt (Ctrl+C). Checks if the singleton
 * UserInterface instance exists. If it does, performs immediate cleanup: sets the atomic
 * running flag to 0 to signal the main event loop to stop, calls endwin() to restore
 * terminal settings and prevent terminal corruption, and exits with EXIT_SUCCESS.
 *
 * Ensures the terminal is properly restored to its original state even when the program
 * terminates abnormally via interrupt. This prevents the terminal from being left in a
 * curses-altered state (e.g., hidden cursor, no echo) after exit.
 *
 * Provides defensive null-check of the singleton instance in case the handler is invoked
 * after the UI is destroyed.
 *
 * Does not require signal re-registration, as SIGINT handler persists across invocations.
 * Terminates the process immediately with exit(); does not return to caller.
 *
 * Enables clean shutdown with guaranteed terminal restoration and prevents resource leaks
 * by terminating the entire process rather than returning to potentially inconsistent state.
 */
void UserInterface::handle_interrupt(int sig) {
    if (UserInterface::instance) {
        UserInterface::instance->running = 0;
        endwin();
        exit(EXIT_SUCCESS);
    }
}

/**
 * Initializes and starts the UserInterface subsystem.
 *
 * Performs full initialization pipeline: calls init_ncurses() to set up the ncurses
 * library, terminal settings, and signal handlers; calls draw_ui() to render the initial
 * full-terminal Unicode box border; sets the atomic running flag to 1 to signal active
 * state; and calls start_render_thread() to spawn the background rendering thread.
 *
 * The rendering thread continuously redraws the UI (output area, input area, spinner)
 * in response to state changes and terminal resize events (resize_flag). The running
 * flag controls the render thread's event loop; setting it to 1 enables the thread to
 * proceed; setting it to 0 causes the thread to exit gracefully.
 *
 * No parameters; operates on the current UserInterface instance state.
 *
 * @return void
 *
 * Initialization status is communicated via successful completion (or
 * program termination if init_ncurses() encounters a fatal error).
 *
 * Acts as the entry point for the UI system. Should be called once after UserInterface
 * instantiation and before any event processing or input handling.
 *
 * Enables the interactive terminal UI with responsive rendering and asynchronous
 * screen updates via the background thread, while the main thread handles user input
 * and application logic.
 */
void UserInterface::run(void) {
    this->init_ncurses();

    this->draw_ui();

    this->running = 1;

    this->start_render_thread();
}

/**
 * Gracefully terminates the UserInterface subsystem and exits the application.
 *
 * @param exit_code Exit status code passed to exit(); typically EXIT_SUCCESS (0) or EXIT_FAILURE (1).
 *
 * Performs an orderly shutdown sequence: prints a farewell message ("🏁 The session has ended."),
 * sleeps for 1000 milliseconds to ensure the message is visible on screen, stops the background
 * render thread via stop_render_thread(), restores terminal settings via cleanup_ncurses(),
 * and exits the process with the provided exit_code.
 *
 * The delay (napms) between printing and thread shutdown allows the message to be displayed
 * before the render thread is terminated and the screen is cleared, ensuring the user sees
 * the farewell notification.
 *
 * Guarantees terminal restoration even when exiting via this controlled path, preventing
 * the terminal from being left in a corrupted ncurses state. The cleanup_ncurses() call
 * ensures endwin() is invoked to restore terminal settings (cursor, echo, etc.).
 *
 * Acts as the primary exit point for controlled application termination. Should be called
 * in response to user quit command (q/Q) or successful completion of application logic.
 *
 * Terminates the process immediately with exit(); does not return to caller.
 *
 * Enables clean, user-facing shutdown with guaranteed terminal restoration and thread cleanup.
 */
void UserInterface::end_session(int exit_code) {
    this->print("🏁 The session has ended.");

    napms(1000);

    this->stop_render_thread();

    this->cleanup_ncurses();

    exit(exit_code);
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
 * Prints the formatted buffer via this->print() with MESSAGE_TYPE_NORMAL styling, displaying
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

    this->print(buffer, UserInterface::MESSAGE_TYPE_NORMAL);
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
 * Outputs the constructed buffer to the ncurses UI using print() with MESSAGE_TYPE_NORMAL
 * styling, ensuring the model list appears in the output area and is preserved in the UI's
 * output_buffer for proper rendering and scrolling.
 *
 * @note This method assumes models is not empty; calling with an empty vector will display
 * only the header message without any selectable options. Typically called after successfully
 * retrieving the model list from the Ollama server via query_model_list().
 *
 * @see print(), MESSAGE_TYPE_NORMAL
 */
void UserInterface::print_model_list(const std::vector<std::string>& models) {
    std::string buffer;
    buffer += "\n🛈 Select a model from the list below:\n";
    for (size_t i = 0; i < models.size(); i += 1) {
        buffer += "[" + std::to_string(i) + "] " + models[i] + "\n";
    }

    this->print(buffer, UserInterface::MESSAGE_TYPE_NORMAL);
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
        exit(1);
    }

    this->request_redraw();

    this->input_buffer.clear();
    int ch;

    while ((ch = getch()) != '\n') {
        if (ch == KEY_BACKSPACE || ch == 127) {
            if (!input_buffer.empty()) {
                input_buffer.pop_back();
                input_cursor_x--;
                this->request_redraw();
            }
        } else if (ch != ERR && isprint(ch)) {
            if (input_buffer.length() < COLS - 40) { // Leave room for prompt
                input_buffer.push_back(ch);
                input_cursor_x++;
                this->request_redraw();
            }
        }
    }

    if (this->input_buffer == "q" || this->input_buffer == "Q") {
        this->end_session(EXIT_SUCCESS);
    }

    index = stoi(this->input_buffer);

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
        exit(1);
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
 * @brief Captures the next user prompt via interactive keyboard input and detects quit commands.
 *
 * @param prompt A pointer to a string where the captured prompt will be stored.
 *
 * @return bool True if the user entered "q" or "Q" (quit command); false otherwise.
 *
 * Clears the input buffer and enters a getch() loop to capture keyboard input, supporting
 * backspace (KEY_BACKSPACE or 127) and printable characters with length constraints.
 * Input terminates on newline (Enter key). Copies the input buffer to prompt and returns
 * true if the prompt is a quit command, signaling session termination.
 *
 * @note Input buffer length is capped at COLS - 40. Requests a UI redraw on each keypress.
 */
bool UserInterface::next_prompt(std::string* prompt) {
    bool done = false;

    this->request_redraw();

    this->input_buffer.clear();
    int ch;

    while ((ch = getch()) != '\n') {
        if (ch == KEY_BACKSPACE || ch == 127) {
            if (!input_buffer.empty()) {
                input_buffer.pop_back();
                input_cursor_x--;
                this->request_redraw();
            }
        } else if (ch != ERR && isprint(ch)) {
            if (input_buffer.length() < COLS - 40) {
                input_buffer.push_back(ch);
                input_cursor_x++;
                this->request_redraw();
            }
        }
    }

    *prompt = this->input_buffer;

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
        exit(1);
    }

    if (*prompt == "q" || *prompt == "Q") {
        done = true;
    }

    return done;
}

/**
 * @brief Queues a typed message to the output buffer and requests a UI redraw.
 *
 * @param text The message content to be displayed.
 * @param type The message type (MESSAGE_TYPE_NORMAL, MESSAGE_TYPE_ERROR, MESSAGE_TYPE_SUCCESS) that controls styling.
 * Defaults to MESSAGE_TYPE_NORMAL if not specified.
 * @param stream In the stream mode, every chunk of text after the first one is appended to the last buffer entry.
 * Defaults to 'false'.
 *
 * Thread-safe: acquires ui_mutex before adding the message to output_buffer.
 * Automatically requests a redraw via request_redraw() to display the new message.
 *
 * @see draw_output_area()
 */
void UserInterface::print(const std::string& text, int type, bool stream) {
    {
        std::lock_guard<std::mutex> lock(this->ui_mutex);
        if (stream == true && !output_buffer.empty()) {
            output_buffer.back().first += text;
        } else {
            output_buffer.push_back({text, type});
        }
    }
    this->request_redraw();
}

/**
 * @brief Queues an error message (bold red with ❌ prefix) and requests a redraw.
 *
 * @param text The error message content.
 *
 * @see print()
 */
void UserInterface::print_error(const std::string& text) {
    this->print(text, UserInterface::MESSAGE_TYPE_ERROR);
}

/**
 * @brief Queues a success message (bold with ✅ prefix) and requests a redraw.
 *
 * @param text The success message content.
 *
 * @see print()
 */
void UserInterface::print_success(const std::string& text) {
    this->print(text, UserInterface::MESSAGE_TYPE_SUCCESS);
}

/**
 * @brief Queues a normal message (plain text) and requests a redraw.
 *
 * @param text The message content.
 * @param stream In the stream mode, every chunk of text after the first one is appended to the last buffer entry.
 * Defaults to 'false'.
 *
 * @see print()
 */
void UserInterface::print_message(const std::string& text, bool stream) {
    this->print(text, UserInterface::MESSAGE_TYPE_NORMAL, stream);
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
    std::cerr << "❌ " << text;
    std::cerr.flush();
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
    std::cout << "✅ " << text;
    std::cout.flush();
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
    std::cout << text;
    std::cout.flush();
}

/**
 * @brief Signals the render thread to perform a UI redraw on the next cycle.
 *
 * Sets should_redraw to true and notifies cv_redraw to wake the render loop,
 * ensuring a timely UI update without waiting for the 1500ms timeout.
 *
 * @note Thread-safe: uses atomic operations and condition variable signaling.
 * @see render_loop()
 */
void UserInterface::request_redraw(void) {
    this->should_redraw = true;
    this->cv_redraw.notify_one();
}

/**
 * @brief Activates the spinner animation and triggers a UI redraw.
 *
 * Acquires ui_mutex, sets spinner_active to true, resets spinner_frame to 0,
 * and sets should_redraw to true to display the spinner on the next render cycle.
 *
 * @note Thread-safe: acquires exclusive lock via lock_guard before modifying state.
 * @see stop_spinner(), draw_spinner()
 */
void UserInterface::start_spinner() {
    std::lock_guard<std::mutex> lock(ui_mutex);
    this->spinner_active = true;
    this->spinner_frame = 0;
    this->should_redraw = true;
}

/**
 * @brief Deactivates the spinner animation and triggers a UI redraw.
 *
 * Acquires ui_mutex, sets spinner_active to false, resets spinner_frame to 0,
 * and sets should_redraw to true to clear the spinner on the next render cycle.
 *
 * @note Thread-safe: acquires exclusive lock via lock_guard before modifying state.
 * @see start_spinner(), draw_spinner()
 */
void UserInterface::stop_spinner() {
    std::lock_guard<std::mutex> lock(ui_mutex);
    this->spinner_active = false;
    this->spinner_frame = 0;
    this->should_redraw = true;
}

/**
 * @brief Increments the spinner frame counter if the spinner is active.
 *
 * If spinner_active is true, advances spinner_frame with modulo 4 (cycling
 * through frames 0–3 to rotate the spinner characters |, /, -, \) and signals
 * a redraw to display the new frame.
 *
 * @note Called by render loop to update spinner animation each cycle.
 * @see is_spinner_active(), draw_spinner()
 */
void UserInterface::advance_spinner_frame() {
    if (this->is_spinner_active()) {
        this->spinner_frame = (spinner_frame + 1) % 4;
        this->should_redraw = true;
    }
}

/**
 * @brief Atomically queries whether the spinner animation is currently active.
 *
 * Returns the current value of the spinner_active atomic flag.
 *
 * @return true if spinner is active, false otherwise.
 * @note Thread-safe: uses atomic load for safe concurrent access.
 * @see start_spinner(), stop_spinner()
 */
bool UserInterface::is_spinner_active() const {
    return spinner_active.load();
}

/**
 * @brief Updates the progress percentage displayed with the spinner.
 *
 * Clamps the provided percent value to the range [0, 100] and assigns it to
 * progress_percent, then signals a redraw to reflect the updated progress.
 *
 * @param percent Progress value to set; automatically clamped to [0, 100].
 * @note Clipping prevents invalid percentage values from being displayed.
 * @see draw_spinner()
 */
void UserInterface::set_progress(int percent) {
    this->progress_percent = std::clamp(percent, 0, 100);
    this->should_redraw = true;
}
