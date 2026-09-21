#!/bin/bash

source tools/shell/utils/config.sh
source tools/shell/utils/print-utils.sh ''

##
# Prints the script usage instuctions.
##
print_usage() {
  print_info_title "<< ${0} usage >>"
  print_usage_tip "bash tools/shell/install.sh" "print help"
  print_usage_tip "bash tools/shell/install.sh all" "install core tools, commitizen, and shellcheck on Linux"
  print_usage_tip "bash tools/shell/install.sh all osx" "install core tools, commitizen, and shellcheck on OSX"
  print_usage_tip "bash tools/shell/install.sh commitizen" "install commitizen on Linux"
  print_usage_tip "bash tools/shell/install.sh commitizen osx" "install commitizen on OSX"
  print_usage_tip "bash tools/shell/install.sh shellcheck" "install shellcheck on Linux"
  print_usage_tip "bash tools/shell/install.sh shellcheck osx" "install shellcheck on OSX"
  print_gap
}

##
# Core tools for Linux.
##
install_core_tools_linux() {
  sudo apt install -y build-essential git cmake g++ gdb clang-format clang-tidy cppcheck
  print_gap

  git --version
  print_gap

  cmake --version
  print_gap

  g++ --version
  print_gap

  gdb --version
  print_gap
}

##
# Core tools for OSX.
##
install_core_tools_osx() {
  brew install cmake gcc git
}

install_core_tools() {
  if [ "$1" = "osx" ]; then
    install_commitizen_osx
  else
    install_core_tools_linux
  fi
}

##
# Install commitizen, cz-conventional-changelog, and npm-check-updates on Linux.
# It is assumed that NodeJS is installed.
##
install_commitizen_linux() {
  print_info_title "<< Intalling commitizen, cz-conventional-changelog, and npm-check-updates via NPM globaly >>"
  print_gap

  sudo npm install -g commitizen@latest cz-conventional-changelog@latest npm-check-updates@latest || exit 1
}

##
# Install commitizen on OSX.
# It is assumed that Python is installed.
##
install_commitizen_osx() {
  print_info_title "<< Intalling commitizen via pypi globally >>"
  print_gap

  brew install commitizen || exit 1
}

##
# Install global dependencies.
# Ref: https://commitizen.github.io/cz-cli/
##
install_commitizen() {
  if [ "$1" = "osx" ]; then
    install_commitizen_osx
  else
    install_commitizen_linux
  fi
}

##
# Install shellcheck on Linux.
##
install_shellcheck_linux() {
  print_info_title "<< Installing shellcheck on Linux >>"
  print_gap

  sudo apt -y install shellcheck
}

##
# Install shellcheck on OSX.
##
install_shellcheck_osx() {
  print_info_title "<< Installing shellcheck on OSX >>"
  print_gap

  brew install shellcheck
}

##
# Install shellcheck.
# Ref: https://www.shellcheck.net/
##
install_shellcheck() {
  if [ "$1" = "osx" ]; then
    install_shellcheck_osx
  else
    install_shellcheck_linux
  fi
  shellcheck --version
}

##
# Dependencies installation control flow.
##
if [ "$1" = "?" ]; then
  print_usage
elif [ "$1" = "all" ]; then
  install_core_tools "$2"
  install_shellcheck "$2"
  install_commitizen "$2"
elif [ "$1" = "commitizen" ]; then
  install_commitizen "$2"
elif [ "$1" = "shellcheck" ]; then
  install_shellcheck "$2"
else
  print_usage
  exit 1
fi
