#!/bin/bash

set -euo pipefail

clang-format src/* -i -Werror

cppcheck --std=c++20 \
  --enable=all \
  --error-exitcode=1 \
  --suppress=missingIncludeSystem \
  --suppress=unusedLabel \
  -I src/ \
  src/
