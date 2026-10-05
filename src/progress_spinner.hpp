#pragma once

#ifndef PROGRESS_SPINNER_HPP
#define PROGRESS_SPINNER_HPP

#include <atomic>
#include <curl/curl.h>
#include <iostream>
#include <thread>

/**
 * Progress spinner for asynchronous operations.
 *
 * @example manual mode
 * ```cpp
 * ProgressSpinner spinner = ProgressSpinner();
 * spinner.start();
 * curl_easy_perform(curl_handle);
 * spinner.stop();
 * ```
 */
class ProgressSpinner {
  private:
    std::atomic<bool> active{false};
    std::thread spinner_thread;

  public:
    ProgressSpinner();
    explicit ProgressSpinner(bool activate);
    ~ProgressSpinner();

    void start();
    void stop();

    static int progress_callback(void* clientp, curl_off_t dltotal, curl_off_t dlnow,
                                 curl_off_t ultotal, curl_off_t ulnow);
};

#endif
