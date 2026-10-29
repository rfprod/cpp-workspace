#pragma once

#include "interface/user_interface.hpp"

class SystemUtils {
  private:
    /**
     * Pointer to the UserInterface for displaying messages and errors.
     * Used for both CLI and interactive UI output.
     */
    UserInterface* ui;

  public:
    explicit SystemUtils(UserInterface* ui);

    void catch_error(int status);
};
