#include "progress_spinner.hpp"
#include <atomic>
#include <chrono>
#include <curl/curl.h>
#include <iostream>
#include <thread>

ProgressSpinner::ProgressSpinner() : ui(nullptr), active(false) {
}

ProgressSpinner::ProgressSpinner(UserInterface* user_interface, bool activate)
    : ui(user_interface), active(false) {
    if (activate && ui != nullptr) {
        start();
    }
}

ProgressSpinner::~ProgressSpinner() {
    stop();
}

/**
 * Implementation details:
 * - In manual mode, renders spinner characters and progress directly to stdout.
 * - In interactive mode, delegates animation and progress updates to the UserInterface.
 * - Resets completion_shown flag and initializes progress to 0%.
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
            printf("\n");
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
 * Sets active flag to false, waits for the spinner thread to finish,
 * and notifies the UserInterface to stop its spinner if present.
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
 * Implementation notes:
 * - In manual mode, displays progress percentage if total is known, otherwise shows spinner.
 * - In interactive mode, updates both spinner frame and progress bar in the UI.
 * - Returns 0 to allow CURL to continue operation.
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
 * Static callback adapter that casts the clientp void pointer to a ProgressSpinner
 * instance and delegates to the progress_callback_impl() method.
 *
 * @return 0 to continue the operation, 1 if clientp is nullptr.
 */
int ProgressSpinner::progress_callback(void* clientp, curl_off_t dltotal, curl_off_t dlnow,
                                       curl_off_t ultotal, curl_off_t ulnow) {
    ProgressSpinner* self = static_cast<ProgressSpinner*>(clientp);
    if (self == nullptr) {
        return 1;
    }
    return self->progress_callback_impl(dltotal, dlnow);
}
