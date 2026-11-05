#!/bin/bash

set -euo pipefail

# Change to repo root (handles running from any directory)
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
cd "$REPO_ROOT"

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

echo "Checking with clang-format..."
find src tests -type f \( -name "*.cpp" -o -name "*.hpp" \) -exec clang-format -i -Werror {} +

echo "Checking with cppcheck..."
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
