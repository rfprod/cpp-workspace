#include "progress_spinner.hpp"
#include <atomic>
#include <chrono>
#include <curl/curl.h>
#include <iostream>
#include <thread>

/**
 * Constructs a ProgressSpinner in manual mode.
 *
 * Creates a spinner that displays to stdout without UI integration.
 * Must be manually started with start().
 */
ProgressSpinner::ProgressSpinner() : ui(nullptr), active(false) {
}

/**
 * Constructs a ProgressSpinner with optional UI integration.
 *
 * @param user_interface Pointer to a UserInterface instance for rendering,
 *                        or nullptr for manual mode (CLI spinner).
 * @param activate If true and user_interface is not nullptr, automatically
 *                 starts the spinner. Default: false.
 *
 * @note The UserInterface pointer must remain valid for the lifetime of
 *       the ProgressSpinner object.
 */
ProgressSpinner::ProgressSpinner(UserInterface* user_interface, bool activate) : ui(user_interface), active(false) {
    if (activate && ui != nullptr) {
        start();
    }
}

/**
 * Destructs the ProgressSpinner, ensuring the spinner thread is stopped.
 *
 * Calls stop() to cleanly shut down the spinner thread.
 */
ProgressSpinner::~ProgressSpinner() {
    stop();
}

/**
 * Starts the spinner animation on a background thread.
 *
 * - If no UserInterface is set, displays an animated spinner (| / - \) to stdout.
 * - If a UserInterface is set, delegates animation to the UI via start_spinner()
 *   and advance_spinner_frame().
 *
 * Sets progress to 0% and resets the completion_shown flag.
 *
 * @note Safe to call multiple times; subsequent calls are no-ops if already running.
 */
void ProgressSpinner::start() {
    this->completion_shown = false;
    if (this->ui != nullptr) {
        this->ui->set_progress(0);
    }
    this->active = true;

    if (this->ui == nullptr || this->ui->is_running() == false) {
        this->spinner_thread = std::thread([this]() {
            const char spinner_chars[] = {'|', '/', '-', '\\'};
            int i = 0;
            while (this->active) {
                std::cout << "\r" << spinner_chars[i++ % 4] << " Loading..." << std::flush;
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
            }
            UserInterface::print_message_noninteractive("\n");
        });
    } else {
        this->ui->start_spinner();

        this->spinner_thread = std::thread([this]() {
            while (this->active) {
                ui->advance_spinner_frame();
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
            }
        });
    }
}

/**
 * Stops the spinner animation and cleans up the background thread.
 *
 * Waits for the spinner thread to finish (via join()) and notifies the
 * UserInterface to stop its spinner, if set.
 *
 * @note Safe to call multiple times or if start() was never called.
 */
void ProgressSpinner::stop() {
    this->active = false;
    if (this->spinner_thread.joinable()) {
        this->spinner_thread.join();
    }
    if (this->ui != nullptr) {
        this->ui->stop_spinner();
    }
}

/**
 * Internal progress callback for CURL integration.
 *
 * Processes download progress information and updates the spinner or UI accordingly.
 * Displays a progress bar (▰▰▰▱) with percentage if total size is known.
 *
 * @param dltotal Total bytes to download (0 if unknown).
 * @param dlnow Bytes downloaded so far.
 *
 * @return 0 to continue the CURL operation, non-zero to abort.
 *
 * @note This method is called frequently by CURL; keep it lightweight.
 */
int ProgressSpinner::progress_callback_impl(curl_off_t dltotal, curl_off_t dlnow) {
    if (this->ui == nullptr || !this->ui->is_running()) {
        const char spinner_chars[] = {'|', '/', '-', '\\'};

        if (dltotal > 0) {
            int progress = (dlnow * 100) / dltotal;
            std::cout << "\r▰▰▰▱ Progress: " << progress << "%     " << std::flush;
        } else {
            static int i = 0;
            std::cout << "\r" << spinner_chars[i++ % 4] << " Loading..." << std::flush;
        }
        return 0;
    }

    this->ui->advance_spinner_frame();

    if (dltotal > 0) {
        int progress = (dlnow * 100) / dltotal;
        this->ui->set_progress(progress);
    }

    return 0;
}

/**
 * Static CURL callback adapter for progress tracking.
 *
 * This static method serves as the callback function for CURL's
 * CURLOPT_XFERINFOFUNCTION option. It casts the clientp parameter to a
 * ProgressSpinner instance and delegates to progress_callback_impl().
 *
 * @param clientp Opaque pointer to the ProgressSpinner instance (passed via
 *                CURLOPT_XFERINFODATA).
 * @param dltotal Total bytes to download (0 if unknown).
 * @param dlnow Bytes downloaded so far.
 * @param ultotal Total bytes to upload (unused).
 * @param ulnow Bytes uploaded so far (unused).
 *
 * @return 0 to continue the CURL operation, 1 to abort if clientp is nullptr.
 *
 * @note CURL calls this frequently; ensure your clientp points to a valid
 *       ProgressSpinner instance.
 */
int ProgressSpinner::progress_callback(void* clientp, curl_off_t dltotal, curl_off_t dlnow, curl_off_t ultotal,
                                       curl_off_t ulnow) {
    ProgressSpinner* self = static_cast<ProgressSpinner*>(clientp);
    if (self == nullptr) {
        return 1;
    }
    return self->progress_callback_impl(dltotal, dlnow);
}
