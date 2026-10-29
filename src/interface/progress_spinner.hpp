#pragma once

#include "user-interface.hpp"
#include <atomic>
#include <curl/curl.h>
#include <iostream>
#include <thread>

/**
 * Displays a progress indicator for asynchronous operations, with optional UI integration.
 *
 * ProgressSpinner supports two modes:
 * - **Manual mode**: Displays an animated spinner to stdout for CLI applications.
 * - **Interactive mode**: Integrates with a UserInterface instance for display within
 *   a graphical or TUI framework.
 *
 * The spinner runs on a separate thread and updates independently of the main operation.
 * Designed to work seamlessly with CURL operations via the static progress_callback method.
 *
 * @note Thread-safe: Uses std::atomic for synchronization without locks.
 *
 * @example Manual mode (CLI spinner):
 * @code
 * ProgressSpinner spinner;
 * spinner.start();
 * curl_easy_perform(curl_handle);
 * spinner.stop();
 * @endcode
 *
 * @example Interactive mode (with UserInterface):
 * @code
 * UserInterface ui;
 * ProgressSpinner spinner(&ui, true);  // auto-starts
 * // UI displays progress and spinner animation
 * curl_easy_perform(curl_handle);
 * spinner.stop();
 * @endcode
 *
 * @example CURL integration with progress tracking:
 * @code
 * ProgressSpinner spinner(&ui);
 * spinner.start();
 *
 * CURL* curl = curl_easy_init();
 * curl_easy_setopt(curl, CURLOPT_XFERINFOFUNCTION, ProgressSpinner::progress_callback);
 * curl_easy_setopt(curl, CURLOPT_XFERINFODATA, &spinner);
 * curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 0L);
 * curl_easy_perform(curl);
 *
 * spinner.stop();
 * curl_easy_cleanup(curl);
 * @endcode
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
    /**
     * Constructs a ProgressSpinner in manual mode.
     *
     * Creates a spinner that displays to stdout without UI integration.
     * Must be manually started with start().
     */
    ProgressSpinner();

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
    explicit ProgressSpinner(UserInterface* user_interface, bool activate = false);

    /**
     * Destructs the ProgressSpinner, ensuring the spinner thread is stopped.
     *
     * Calls stop() to cleanly shut down the spinner thread.
     */
    ~ProgressSpinner();

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
    void start();

    /**
     * Stops the spinner animation and cleans up the background thread.
     *
     * Waits for the spinner thread to finish (via join()) and notifies the
     * UserInterface to stop its spinner, if set.
     *
     * @note Safe to call multiple times or if start() was never called.
     */
    void stop();

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
    int progress_callback_impl(curl_off_t dltotal, curl_off_t dlnow);

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
    static int progress_callback(void* clientp, curl_off_t dltotal, curl_off_t dlnow,
                                 curl_off_t ultotal, curl_off_t ulnow);
};
