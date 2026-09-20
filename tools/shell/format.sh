#!/bin/bash

set -euo pipefail

clang-format src/* -i -Werror

cppcheck --enable=all --error-exitcode=1 --suppress=missingIncludeSystem src/
