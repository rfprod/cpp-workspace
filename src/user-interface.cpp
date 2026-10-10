#include "user-interface.hpp"
#include <chrono>
#include <csignal>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <locale>
#include <ncurses.h>
#include <sstream>

UserInterface* UserInterface::instance = nullptr;

constexpr int COLOR_PAIR_DEFAULT = 1;
constexpr int COLOR_PAIR_INVERT = 2;

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
        start_color();
        init_pair(COLOR_PAIR_DEFAULT, COLOR_WHITE, COLOR_BLACK);
        init_pair(COLOR_PAIR_INVERT, COLOR_BLACK, COLOR_WHITE);
        bkgd(COLOR_PAIR(COLOR_PAIR_DEFAULT));
    }

    signal(SIGWINCH, UserInterface::handle_resize);
    signal(SIGINT, UserInterface::handle_interrupt);
}

void UserInterface::cleanup_ncurses(void) {
    endwin();
}

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

void UserInterface::clear_input_area() {
    int max_y, max_x;
    getmaxyx(stdscr, max_y, max_x);

    mvhline(layout.input_y, 1, ' ', max_x - 2);
    mvhline(layout.input_y + 1, 1, ' ', max_x - 2);
}

void UserInterface::draw_input_area() {
    this->clear_input_area();
    if (this->is_spinner_active()) {
        return;
    }

    mvprintw(layout.input_y, layout.input_x, "⏳ Type your prompt (q/Q to quit): ");

    // Draw the actual input buffer that user typed
    mvprintw(layout.input_y, layout.input_x + 37, "%s", input_buffer.c_str());
}

UserInterface::UserInterface() : resize_flag(0), running(0), input_cursor_x(0), layout({}) {
    UserInterface::instance = this;
}

UserInterface::~UserInterface() {
    this->cleanup_ncurses();
    UserInterface::instance = nullptr;
}

bool UserInterface::is_running() {
    return this->running == 1;
}

void UserInterface::handle_resize(int sig) {
    if (UserInterface::instance) {
        UserInterface::instance->resize_flag = 1;
    }

    signal(SIGWINCH, UserInterface::handle_resize);
}

void UserInterface::handle_interrupt(int sig) {
    if (UserInterface::instance) {
        UserInterface::instance->running = 0;
        endwin();
        exit(EXIT_SUCCESS);
    }
}

void UserInterface::run(void) {
    this->init_ncurses();

    this->draw_ui();

    this->running = 1;

    this->start_render_thread();
}

void UserInterface::end_session(int exit_code) {
    this->print("🏁 The session has ended.");

    napms(1000);

    this->stop_render_thread();

    this->cleanup_ncurses();

    exit(exit_code);
}

void UserInterface::print_args(const std::string& model, const std::string& prompt, bool stream,
                               bool interactive) {
    std::string buffer;
    buffer += "🛈 model = " + model + "\n";
    buffer += "🛈 prompt = " + prompt + "\n";
    buffer += "🛈 stream = " + std::string(stream ? "true" : "false") + "\n";
    buffer += "🛈 interactive = " + std::string(interactive ? "true" : "false") + "\n";

    this->print(buffer, UserInterface::MESSAGE_TYPE_NORMAL);
}

void UserInterface::print_args_noninteractive(const std::string& model, const std::string& prompt,
                                              bool stream, bool interactive) {
    printf("\n");
    printf("🛈 model = %s\n", model.c_str());
    printf("🛈 prompt = %s\n", prompt.c_str());
    printf("🛈 stream = %s\n", stream ? "true" : "false");
    printf("🛈 interactive = %s\n", interactive ? "true" : "false");
    printf("\n");
}

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
            if (input_buffer.length() < COLS - 40) { // Leave room for prompt
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

void UserInterface::print(const std::string& text, int type) {
    {
        std::lock_guard<std::mutex> lock(this->ui_mutex);
        output_buffer.push_back({text, type});
    }
    this->request_redraw();
}

void UserInterface::print_error(const std::string& text) {
    this->print(text, UserInterface::MESSAGE_TYPE_ERROR);
}

void UserInterface::print_success(const std::string& text) {
    this->print(text, UserInterface::MESSAGE_TYPE_SUCCESS);
}

void UserInterface::print_message(const std::string& text) {
    this->print(text, UserInterface::MESSAGE_TYPE_NORMAL);
}

void UserInterface::draw_output_area() {
    // Clear the output area
    for (int i = 0; i < layout.output_height; i++) {
        mvhline(layout.output_y + i, 1, ' ', COLS - 2);
    }

    int current_line = layout.input_y - 2; // Start from bottom
    int max_content_width = COLS - 4;

    // Iterate backwards through buffer (newest first)
    for (auto it = output_buffer.rbegin(); it != output_buffer.rend(); ++it) {
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
        for (auto line_it = display_lines.rbegin(); line_it != display_lines.rend(); ++line_it) {
            if (current_line <= layout.output_y)
                break;

            if (type == UserInterface::MESSAGE_TYPE_ERROR) {
                wattron(stdscr, COLOR_PAIR(COLOR_PAIR_INVERT) | A_BOLD);
                mvprintw(current_line, 2, "❌ %s", line_it->c_str());
                wattroff(stdscr, COLOR_PAIR(COLOR_PAIR_INVERT) | A_BOLD);
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

void UserInterface::start_render_thread(void) {
    this->thread_running = true;
    this->render_thread = std::thread(&UserInterface::render_loop, this);
}

void UserInterface::stop_render_thread(void) {
    this->thread_running = false;
    this->cv_redraw.notify_one();
    if (this->render_thread.joinable()) {
        this->render_thread.join();
    }
}

void UserInterface::request_redraw(void) {
    this->should_redraw = true;
    this->cv_redraw.notify_one();
}

void UserInterface::render_loop(void) {
    while (this->thread_running) {
        std::unique_lock<std::mutex> lock(this->ui_mutex);

        // Wait for redraw request or timeout (100ms)
        this->cv_redraw.wait_for(lock, std::chrono::milliseconds(1500),
                                 [this] { return this->should_redraw.load(); });

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

void UserInterface::start_spinner() {
    std::lock_guard<std::mutex> lock(ui_mutex);
    this->spinner_active = true;
    this->spinner_frame = 0;
    this->should_redraw = true;
}

void UserInterface::stop_spinner() {
    std::lock_guard<std::mutex> lock(ui_mutex);
    this->spinner_active = false;
    this->spinner_frame = 0;
    this->should_redraw = true;
}

void UserInterface::advance_spinner_frame() {
    if (this->is_spinner_active()) {
        this->spinner_frame = (spinner_frame + 1) % 4;
        this->should_redraw = true;
    }
}

bool UserInterface::is_spinner_active() const {
    return spinner_active.load();
}

void UserInterface::set_progress(int percent) {
    this->progress_percent = std::clamp(percent, 0, 100);
    this->should_redraw = true;
}

void UserInterface::draw_spinner() {
    if (!this->is_spinner_active()) {
        return;
    }

    const char spinner_chars[] = {'|', '/', '-', '\\'};
    int frame = this->spinner_frame.load();
    int progress = this->progress_percent.load();

    attron(COLOR_PAIR(COLOR_PAIR_DEFAULT));

    if (progress > 0) {
        mvprintw(this->layout.spinner_y, this->layout.spinner_x, "%c Progress: %d%%",
                 spinner_chars[frame], progress);
    } else {
        mvprintw(this->layout.spinner_y, this->layout.spinner_x, "%c Loading...",
                 spinner_chars[frame]);
    }

    attroff(COLOR_PAIR(COLOR_PAIR_DEFAULT));
}
