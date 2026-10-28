#!/bin/bash

set -euo pipefail

# Change to repo root (handles running from any directory)
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
cd "$REPO_ROOT"

echo "Running from: $(pwd)"
echo "---"
echo "Directory structure:"
ls -la src/
echo "---"
echo "All .cpp and .hpp files:"
find src -type f \( -name "*.cpp" -o -name "*.hpp" \)
echo "---"

find src -type f \( -name "*.cpp" -o -name "*.hpp" \) -exec clang-format -i -Werror {} +

cppcheck --std=c++20 \
  --enable=all \
  --error-exitcode=1 \
  --suppress=missingIncludeSystem \
  --suppress=unusedLabel \
  -I src/ \
  -I src/interface \
  -I src/ollama \
  -I src/utils \
  src/
