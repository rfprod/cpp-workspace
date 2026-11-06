#!/bin/bash

set -euo pipefail

# Change to repo root (handles running from any directory)
ensure_repo_root() {
  local SCRIPT_DIR
  SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

  local REPO_ROOT
  REPO_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
  cd "$REPO_ROOT"
}
ensure_repo_root

print_diag() {
  echo "Running from: $(pwd)"
  echo "---"
  echo "Directory structure [src]:"
  ls -la src/
  echo "---"
  echo "Directory structure [tests]:"
  ls -la tests/
  echo "---"
  echo "All .cpp and .hpp files [src]:"
  find src tests -type f \( -name "*.cpp" -o -name "*.hpp" \)
  echo "---"
  echo "All .cpp and .hpp files [tests]:"
  find tests -type f \( -name "*.cpp" -o -name "*.hpp" \)
  echo "---"
}
print_diag

clang_format_check() {
  local VERSION
  VERSION=$(clang-format --version)
  echo "Checking with clang-format [${VERSION}]..."
  find src tests -type f \( -name "*.cpp" -o -name "*.hpp" \) -exec clang-format -i -Werror {} +
}
clang_format_check

cppcheck_check() {
  local VERSION
  VERSION=$(cppcheck --version)
  echo "Checking with cppcheck [${VERSION}]..."
  cppcheck --std=c++20 \
    --enable=all \
    --error-exitcode=1 \
    --suppress=missingIncludeSystem \
    --suppress=unusedLabel \
    -I src/ \
    -I src/interface \
    -I src/ollama \
    -I src/storage \
    -I src/utils \
    -I tests/ \
    src/ tests/
}
cppcheck_check
