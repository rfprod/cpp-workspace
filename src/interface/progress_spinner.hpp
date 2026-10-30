#pragma once

#include "user_interface.hpp"
#include <atomic>
#include <curl/curl.h>
#include <iostream>
#include <thread>

/**
 * Displays and manages an animated progress spinner with optional download tracking.
 *
 * Runs a spinner animation in a dedicated background thread, providing visual feedback
 * during network operations. Supports two modes: UI-driven (displays via UserInterface)
 * and manual mode (spinner_thread active but output deferred). Integrates with CURL
 * progress callbacks to track download progress. Thread-safe via atomic flags for
 * state management (active, completion_shown).
 */
class ProgressSpinner {
  private:
    /**
     * Pointer to the associated UserInterface, or nullptr for manual mode.
     */
    UserInterface* ui;

    /**
     * Flag indicating whether the spinner is currently active.
     */
    std::atomic<bool> active{false};

    /**
     * Thread running the spinner animation loop.
     */
    std::thread spinner_thread;

    /**
     * Flag tracking whether a completion message has been displayed.
     */
    std::atomic<bool> completion_shown{false};

  public:
    ProgressSpinner();

    explicit ProgressSpinner(UserInterface* user_interface, bool activate = false);

    ~ProgressSpinner();

    void start();

    void stop();

    int progress_callback_impl(curl_off_t dltotal, curl_off_t dlnow);

    static int progress_callback(void* clientp, curl_off_t dltotal, curl_off_t dlnow, curl_off_t ultotal,
                                 curl_off_t ulnow);
};
