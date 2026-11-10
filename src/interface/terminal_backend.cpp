#include "terminal_backend.hpp"
#include "utils/exit_request.hpp"
#include <chrono>
#include <csignal>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <locale>
#include <ncurses.h>
#include <sstream>
#include <stdexcept>
#include <utility>

std::atomic<bool> TerminalBackend::interrupt_flag{false};
std::atomic<bool> TerminalBackend::resize_flag{false};

static_assert(std::atomic<bool>::is_always_lock_free, "std::atomic<bool> must be lock-free for use in signal handlers");

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
 * This method should be called early during TerminalBackend initialization, before
 * any colored text is rendered to the terminal.
 */
void TerminalBackend::init_dark_theme(void) {
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
 * This method should be called early during TerminalBackend initialization, before
 * any colored text is rendered to the terminal. Select init_dark_theme() or
 * init_light_theme() based on the user's terminal background preference.
 */
void TerminalBackend::init_light_theme(void) {
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
void TerminalBackend::init_ncurses(void) {
    std::string message{};

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
        fprintf(stderr, "Failed to initialize ncurses with TERM=%s\n", term ? term : "unset");
        throw ExitRequest{EXIT_FAILURE};
    }

    this->update_layout();

    // Setup terminal.
    if (cbreak() == ERR || noecho() == ERR) {
        endwin();
        fprintf(stderr, "Terminal setup failed\n");
        throw ExitRequest{EXIT_FAILURE};
    }

    // Enable function keys and arrow keys.
    if (keypad(stdscr, TRUE) == ERR) {
        fprintf(stderr, "Warning: keypad() not supported\n");
    }

    // Block on input (getch waits for input).
    nodelay(stdscr, FALSE);

    // Cursor hiding (may not be supported).
    if (curs_set(0) == ERR) {
        fprintf(stderr, "Warning: cursor control not supported on this terminal\n");
    }

    if (has_colors()) {
        if (this->use_dark_theme == true) {
            this->init_dark_theme();
        } else {
            this->init_light_theme();
        }
        bkgd(COLOR_PAIR(DEFAULT_PAIR));
    }

    this->install_signal_handlers();
}

void TerminalBackend::install_signal_handlers() {
    struct sigaction sa {};
    sigemptyset(&sa.sa_mask);

    sa.sa_handler = TerminalBackend::handle_resize;
    sa.sa_flags = SA_RESTART; // don't interrupt getch() just for a resize
    sigaction(SIGWINCH, &sa, nullptr);

    sa.sa_handler = TerminalBackend::handle_interrupt;
    sa.sa_flags = 0; // no SA_RESTART, so a blocking read returns EINTR
    sigaction(SIGINT, &sa, nullptr);
}

void TerminalBackend::handle_resize(int sig) {
    TerminalBackend::resize_flag.store(true, std::memory_order_relaxed);
}

void TerminalBackend::handle_interrupt(int sig) {
    TerminalBackend::interrupt_flag.store(true, std::memory_order_relaxed);
}

/**
 * Constructs a TerminalBackend instance and initializes member variables.
 *
 * Initializes input_cursor_x to 0 (cursor at leftmost position).
 * Initializes layout struct to default-constructed values (all fields zeroed/default).
 * Sets use_dark_theme to true, establishing dark theme as the default color scheme preference.
 *
 * Note: This constructor performs only member initialization and singleton registration.
 * Full ncurses initialization (terminal setup, color pair configuration, input handling) is deferred
 * to init_ncurses(), which must be called explicitly after construction to establish the UI environment.
 */
TerminalBackend::TerminalBackend() : input_cursor_x(0), layout({}), use_dark_theme(true) {
}

/**
 * Constructs a TerminalBackend instance with a caller-specified color theme preference.
 *
 * @param dark_theme: Boolean flag to select color scheme; true for dark theme, false for light theme.
 *
 * Initializes input_cursor_x to 0 (cursor at leftmost position).
 * Initializes layout struct to default-constructed values (all fields zeroed/default).
 * Sets use_dark_theme to the caller-provided dark_theme value, establishing the preferred
 * color scheme that will be applied during init_ncurses().
 *
 * Note: This constructor performs only member initialization and singleton registration.
 * Full ncurses initialization (terminal setup, color pair configuration, input handling) is deferred
 * to init_ncurses(), which must be called explicitly after construction to establish the UI environment.
 */
TerminalBackend::TerminalBackend(bool dark_theme) : input_cursor_x(0), layout({}), use_dark_theme(dark_theme) {
}

/**
 * Destroys the TerminalBackend and ends its session if it is still active.
 *
 * Calls end_session() with its default exit code. That call stops the render
 * thread and restores the terminal, but only if terminal_restored has not
 * already been set. The destructor does not acquire ui_mutex directly.
 *
 * @post The render thread has been stopped.
 * @post The terminal has been restored, and endwin() has run exactly
 *       once across all calls to end_session().
 *
 * @note Destruction always blocks for about one second because end_session()
 *       calls napms(1000) unconditionally, even when the session was already
 *       ended earlier.
 * @warning Destructors are implicitly noexcept. If end_session() or anything it
 *          calls throws, std::terminate() is called. Call end_session()
 *          explicitly before destruction if a session needs to be ended while
 *          exceptions are still being handled.
 */
TerminalBackend::~TerminalBackend() {
    this->end_session();
}

/**
 * Ends the terminal session and restores the terminal state.
 *
 * The function sleeps for one second, stops the render thread, and then calls
 * endwin() if the terminal has not been restored yet. The check and
 * update of terminal_restored use exchange(), so cleanup runs at most once even
 * if this function is called more than once.
 *
 * @post The render thread has been stopped.
 * @post terminal_restored is true, and endwin() has run exactly once
 *       across all calls.
 *
 * @note The call blocks the calling thread for about one second through
 *       napms(1000) before doing anything else. Avoid calling it from a thread
 *       that must respond promptly.
 */
void TerminalBackend::end_session(int code) {
    napms(500);

    this->stop_render_thread();

    if (!this->terminal_restored.exchange(true)) {
        endwin();
    }

    throw ExitRequest{code};
}

/**
 * Initializes and starts the TerminalBackend subsystem.
 *
 * Performs full initialization pipeline: calls init_ncurses() to set up the ncurses
 * library, terminal settings, and signal handlers; calls draw_ui() to render the initial
 * full-terminal Unicode box border; and calls start_render_thread() to spawn the background rendering thread.
 *
 * The rendering thread continuously redraws the UI (output area, input area, spinner)
 * in response to state changes and terminal resize events (resize_flag).
 *
 * No parameters; operates on the current TerminalBackend instance state.
 *
 * @return void
 *
 * Initialization status is communicated via successful completion (or
 * program termination if init_ncurses() encounters a fatal error).
 *
 * Acts as the entry point for the UI system. Should be called once after TerminalBackend
 * instantiation and before any event processing or input handling.
 *
 * Enables the interactive terminal UI with responsive rendering and asynchronous
 * screen updates via the background thread, while the main thread handles user input
 * and application logic.
 */
void TerminalBackend::run(void) {
    this->init_ncurses();

    this->draw_ui();

    this->start_render_thread();
}

/**
 * Reports whether the backend's thread is currently running.
 *
 * The value is read atomically, so this function does not acquire ui_mutex.
 * The result can become stale as soon as the function returns.
 *
 * @return true if the backend thread is running, false otherwise.
 *
 * @note Use this for status checks only. Do not use it to decide whether to
 *       start or stop the thread without coordinating through another
 *       synchronization mechanism, because the state can change between the
 *       check and the action.
 */
bool TerminalBackend::is_running() {
    return this->thread_running.load();
}

/**
 * Wakes one or all threads waiting on cv_redraw.
 *
 * The function does not acquire ui_mutex and does not modify should_redraw or
 * any other state. Notifying is the only effect.
 *
 * @param all true to wake all waiting threads with notify_all(), false to wake
 *            a single waiting thread with notify_one().
 *
 * @note A waiter that checks a predicate such as should_redraw must update that
 *       predicate before calling this function, or a wakeup can be missed.
 * @warning Calling this without first changing the state a waiter depends on
 *          can cause a lost wakeup. Waking one thread may also leave other
 *          waiters asleep if they need to act on the same change, so use
 *          all = true when more than one thread might need to respond.
 */
void TerminalBackend::notify_cv_redraw(bool all) {
    if (all == true) {
        this->cv_redraw.notify_all();
    } else {
        this->cv_redraw.notify_one();
    }
}

/**
 * Moves the input cursor one column to the left or right.
 *
 * Only steps of -1 or +1 are accepted. The function does not check whether the
 * resulting position is within the bounds of input_buffer, and it does not
 * acquire ui_mutex.
 *
 * @param shift The step to apply to input_cursor_x. Must be -1 or 1.
 *
 * @throws std::invalid_argument If shift is any value other than -1 or 1.
 *
 * @pre input_cursor_x is within the range [0, input_buffer.length()].
 * @post input_cursor_x has been moved by shift.
 *
 * @warning The position is not bounds-checked after the shift. Stepping left
 *          at position 0 or right past the end of input_buffer leaves
 *          input_cursor_x out of range, which callers must avoid.
 * @note Access to input_cursor_x is not synchronized. If another thread reads or
 *       modifies the cursor, the caller must hold ui_mutex.
 */
void TerminalBackend::shift_input_cursor_x(int shift) {
    if (shift != -1 && shift != 1) {
        throw std::invalid_argument("Cursor x can be shifted by -1 or +1.");
    }
    this->input_cursor_x += shift;
}

/**
 * Sets the flag that requests a screen redraw.
 *
 * The value is stored atomically, so this function does not acquire ui_mutex.
 * The previous value is discarded.
 *
 * @param value true to request a redraw, false to clear the request.
 *
 * @note This function does not notify cv_redraw. Setting the flag to true does
 *       not wake a thread waiting on that condition variable, so a waiter may
 *       not see the request until it is woken by another call such as print().
 */
void TerminalBackend::set_should_redraw(bool value) {
    std::lock_guard<std::mutex> lock(this->ui_mutex);
    this->should_redraw.store(value);
}

/**
 * Sets whether the spinner is active.
 *
 * The value is stored atomically, so this function does not acquire ui_mutex.
 * The previous value is discarded.
 *
 * @param value true to activate the spinner, false to deactivate it.
 *
 * @note This call does not set should_redraw or notify cv_redraw. Callers that
 *       need the spinner change displayed must trigger a redraw separately.
 */
void TerminalBackend::set_spinner_active(bool value) {
    std::lock_guard<std::mutex> lock(this->ui_mutex);
    this->spinner_active.store(value);
}

/**
 * Sets the progress percentage shown by the terminal.
 *
 * The value is stored atomically, so this function does not acquire ui_mutex.
 * The previous value is discarded. No range check is performed, so callers
 * are responsible for passing a meaningful percentage.
 *
 * @param value The new progress percentage to store.
 *
 * @note Because no lock is taken, a redraw is not triggered by this call.
 *       Callers that need the change displayed must also update output state
 *       and signal cv_redraw through print().
 */
void TerminalBackend::set_spinner_frame(int value) {
    std::lock_guard<std::mutex> lock(this->ui_mutex);
    if (value >= 0 && value <= 4) {
        this->spinner_frame.store(value);
    } else {
        this->spinner_frame.store((spinner_frame + 1) % 4);
    }
}

void TerminalBackend::set_progress_percent(int value) {
    this->progress_percent.store(value);
}

/**
 * Reports whether the spinner is currently active.
 *
 * The value is read atomically, so this function does not acquire ui_mutex.
 * The result can change immediately after the call returns if another thread
 * updates spinner_active.
 *
 * @return true if the spinner is active, false otherwise.
 */
bool TerminalBackend::get_spinner_active() {
    return this->spinner_active.load();
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
void TerminalBackend::draw_ui(void) {
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
 * Returns early if the spinner is not active. Loads the current animation frame
 * index and progress percentage from atomic variables (spinner_frame, progress_percent)
 * in a thread-safe manner. Selects the appropriate spinner character from a rotating set
 * (|, /, -, \) based on the frame index.
 *
 * Enables DEFAULT_PAIR color styling and renders the spinner at the layout-defined position
 * (layout.spinner_y, layout.spinner_x) using mvprintw(). If progress > 0, displays "X Loading..."
 * with the progress percentage; otherwise displays "X Loading..." without percentage. Disables color
 * styling after rendering.
 *
 * Thread-safe via atomic variable loads; does not modify shared state.
 */
void TerminalBackend::draw_spinner() {
    if (!this->spinner_active.load()) {
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
void TerminalBackend::update_layout() {
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
void TerminalBackend::clear_input_area() {
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
void TerminalBackend::draw_input_area() {
    this->clear_input_area();
    if (this->spinner_active.load()) {
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
void TerminalBackend::draw_output_area() {
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

            if (type == TerminalBackend::MESSAGE_TYPE_ERROR) {
                wattron(stdscr, COLOR_PAIR(ERROR_PAIR) | A_BOLD);
                mvprintw(current_line, 2, "❌ %s", line_it->c_str());
                wattroff(stdscr, COLOR_PAIR(ERROR_PAIR) | A_BOLD);
            } else if (type == TerminalBackend::MESSAGE_TYPE_SUCCESS) {
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
void TerminalBackend::start_render_thread(void) {
    this->thread_running.store(true);
    this->render_thread = std::thread([this]() {
        try {
            this->render_loop();
        } catch (...) {
            this->render_exception = std::current_exception();
        }
        this->thread_running.store(false);
    });
}

/**
 * @brief Gracefully shuts down the background rendering thread.
 *
 * Sets thread_running to false and notifies the condition variable cv_redraw to
 * wake the render loop. Joins the thread if it is joinable, ensuring complete cleanup.
 *
 * @note Call this during session shutdown or before destroying the TerminalBackend instance.
 * @see start_render_thread(), render_loop()
 */
void TerminalBackend::stop_render_thread(void) {
    {
        std::lock_guard<std::mutex> lock(this->ui_mutex);
        this->thread_running.store(false);
    }
    this->cv_redraw.notify_all();
    if (this->render_thread.joinable() && this->render_thread.get_id() != std::this_thread::get_id()) {
        this->render_thread.join();
    }
    if (this->render_exception) {
        std::exception_ptr error = std::exchange(this->render_exception, nullptr);
        std::rethrow_exception(error);
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
void TerminalBackend::render_loop() {
    const int INPUT_PROMPT_WIDTH = 37;
    try {
        while (this->thread_running.load(std::memory_order_acquire)) {
            std::unique_lock<std::mutex> lock(ui_mutex);

            // Wake on redraw, resize, interrupt, or shutdown. The 1500 ms timeout is only a fallback.
            this->cv_redraw.wait_for(lock, std::chrono::milliseconds(1500), [this] {
                return this->should_redraw.load(std::memory_order_acquire)
                       || TerminalBackend::resize_flag.load(std::memory_order_acquire)
                       || TerminalBackend::interrupt_flag.load(std::memory_order_acquire)
                       || !this->thread_running.load(std::memory_order_acquire);
            });

            if (TerminalBackend::interrupt_flag.load(std::memory_order_acquire)
                || !this->thread_running.load(std::memory_order_acquire)) {
                endwin();
                break; // lock is released by the destructor; end_session runs on the main thread
            }

            if (TerminalBackend::resize_flag.exchange(false, std::memory_order_acq_rel)) {
                endwin();
                refresh();
                this->update_layout();
                this->input_cursor_x = layout.input_x + INPUT_PROMPT_WIDTH + input_buffer.length();
                this->should_redraw.store(true, std::memory_order_release);
            }

            if (this->should_redraw.load(std::memory_order_acquire)) {
                this->draw_ui();
                this->draw_output_area();
                this->draw_input_area();
                this->draw_spinner();
                refresh();
                this->should_redraw.store(false, std::memory_order_release);
            }
        }
    } catch (...) {
        std::lock_guard<std::mutex> g(exception_mutex);
        this->render_exception = std::current_exception();
        this->thread_running.store(false, std::memory_order_release);
    }
}

/**
 * Adds text to the output buffer and requests a redraw.
 *
 * When stream is true and the output buffer is not empty, text is appended to
 * the text of the last entry, and type is ignored. Otherwise, a new entry
 * holding text and type is added to the end of the buffer. The buffer is
 * modified while ui_mutex is held.
 *
 * After the buffer is updated, the function sets should_redraw to true and
 * wakes one thread waiting on cv_redraw. The lock is released before the
 * notification is sent.
 *
 * @param text   The text to add to the output buffer.
 * @param type   The category of the new entry. It is ignored when text is
 *               appended to an existing entry.
 * @param stream If true, text continues the most recent entry instead of
 *               starting a new one.
 */
void TerminalBackend::print(const std::string& text, int type, bool stream) {
    {
        std::lock_guard<std::mutex> lock(this->ui_mutex);
        if (stream == true && !output_buffer.empty()) {
            output_buffer.back().first += text;
        } else {
            output_buffer.push_back({text, type});
        }
    }
    this->should_redraw.store(true);
    this->cv_redraw.notify_one();
}

/**
 * Removes all characters from the input buffer.
 *
 * The buffer is empty after the call. Calling the function on an empty buffer
 * has no effect. Any view previously returned by get_input_buffer is invalid
 * after this call. The function does not acquire ui_mutex, so callers that
 * share input_buffer across threads must synchronize access themselves.
 */
void TerminalBackend::clear_input_buffer() {
    this->input_buffer.clear();
}

/**
 * Reports whether the input buffer holds an exit command.
 *
 * The buffer matches only when its entire contents are a single "q" or "Q".
 * A buffer that contains either character along with other input does not
 * match. The function does not acquire ui_mutex, so callers that share
 * input_buffer across threads must synchronize access themselves.
 *
 * @return true if input_buffer is exactly "q" or "Q", false otherwise.
 */
bool TerminalBackend::exit_keys_in_input_buffer() {
    return this->input_buffer == "q" || this->input_buffer == "Q";
}

/**
 * Reports whether the input buffer leaves room for a prompt.
 *
 * The buffer has room for a prompt when its length is less than the terminal
 * width (COLS) minus 40 columns, which is reserved for the prompt. COLS is
 * read at each call, so the result follows terminal resizes.
 *
 * The function does not acquire ui_mutex, so callers that share input_buffer
 * across threads must synchronize access themselves.
 *
 * @return true if input_buffer has room for a prompt, false otherwise.
 */
bool TerminalBackend::room_for_prompt_in_input_buffer() {
    const int usable = COLS - 40;
    return usable > 0 && this->input_buffer.length() < static_cast<std::size_t>(usable);
}

/**
 * Reports whether the input buffer contains no characters.
 *
 * The function does not acquire ui_mutex. Its result can be out of date by
 * the time the caller acts on it if another thread modifies input_buffer in
 * the meantime.
 *
 * @return true if input_buffer is empty, false otherwise.
 */
bool TerminalBackend::input_buffer_is_empty() {
    return this->input_buffer.empty();
}

/**
 * Returns a non-owning view of the current input buffer contents.
 *
 * The view refers to the storage of input_buffer and does not copy it. It
 * stays valid only until input_buffer is modified, for example by
 * push_back_input_buffer or pop_back_input_buffer, which can reallocate or
 * shrink the storage. It also remains valid only while this TerminalBackend
 * exists. The function does not acquire ui_mutex, so callers that share
 * input_buffer across threads must synchronize access themselves.
 *
 * @return A view of input_buffer. Copy the contents if they must outlive the
 *         next modification of the buffer.
 */
std::string_view TerminalBackend::get_input_buffer() const {
    return this->input_buffer;
}

/**
 * Removes the last character from the input buffer.
 *
 * The function does not check whether the buffer is empty. Calling it when
 * input_buffer is empty is undefined behavior. Like push_back_input_buffer,
 * it does not acquire ui_mutex, so callers that share input_buffer across
 * threads must synchronize access themselves.
 *
 * @pre input_buffer is not empty.
 */
void TerminalBackend::pop_back_input_buffer() {
    std::lock_guard<std::mutex> lock(this->ui_mutex);
    this->input_buffer.pop_back();
}

/**
 * Appends a character code to the end of the input buffer.
 *
 * The value is stored as passed, without validation or conversion. The
 * function does not acquire ui_mutex, so callers that share input_buffer
 * across threads must synchronize access themselves.
 *
 * @param ch The character code to append to input_buffer.
 */
void TerminalBackend::push_back_input_buffer(int ch) {
    std::lock_guard<std::mutex> lock(this->ui_mutex);
    this->input_buffer.push_back(ch);
}
