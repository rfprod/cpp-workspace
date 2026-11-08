#pragma once

#include "interface/user_interface.hpp"

class SystemUtils {
  private:
    /**
     * Pointer to the UserInterface for displaying messages and errors.
     * Used for both CLI and interactive UI output.
     */
    std::shared_ptr<UserInterface> ui;

    void report_failure(const std::string& message);

  public:
    explicit SystemUtils(std::shared_ptr<UserInterface> ui);

    void catch_error(int status);
};
