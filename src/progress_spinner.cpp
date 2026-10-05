#include "progress_spinner.hpp"
#include <atomic>
#include <chrono>
#include <curl/curl.h>
#include <iostream>
#include <thread>

ProgressSpinner::ProgressSpinner() {
    this->active = false;
}

ProgressSpinner::ProgressSpinner(bool activate) {
    if (activate == true) {
        this->active = activate;
        start();
    }
}

ProgressSpinner::~ProgressSpinner() {
    stop();
}

// Starts the progress spinner.
void ProgressSpinner::start() {
    active = true;
    spinner_thread = std::thread([this]() {
        const char spinner_chars[] = {'|', '/', '-', '\\'};
        int i = 0;
        while (active) {
            std::cout << "\r" << spinner_chars[i++ % 4] << " Loading..." << std::flush;
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
        std::cout << "\r✓ Complete\n" << std::endl;
    });
}

// Stops the progress spinner.
void ProgressSpinner::stop() {
    active = false;
    if (spinner_thread.joinable()) {
        spinner_thread.join();
    }
}

/**
 * CURL progress callback.
 *
 * @example
 * ```cpp
 * CURL* curl = curl_easy_init();
 * curl_easy_setopt(this->curl, CURLOPT_XFERINFOFUNCTION, ProgressSpinner::progress_callback);
 * curl_easy_setopt(this->curl, CURLOPT_NOPROGRESS, 0L);
 * curl_easy_perform(curl);
 * ```
 */
int ProgressSpinner::progress_callback(void* clientp, curl_off_t dltotal, curl_off_t dlnow,
                                       curl_off_t ultotal, curl_off_t ulnow) {
    if (dltotal > 0) {
        // If size is known, show progress bar
        int progress = (dlnow * 100) / dltotal;
        if (progress == 100) {
            std::cout << "\r✓ Complete  " << std::flush;
        } else {
            std::cout << "\rProgress: " << progress << "% " << std::flush;
        }
    } else {
        // Indeterminate - show spinner
        const char spinner_chars[] = {'|', '/', '-', '\\'};
        static int i = 0;
        std::cout << "\r" << spinner_chars[i++ % 4] << " Loading..." << std::flush;
    }
    return 0;
}
