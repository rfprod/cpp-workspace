#!/bin/bash

set -euo pipefail

# Color definitions
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m'

# Help function
show_help() {
  echo -e "${BLUE}Usage: ./tools/build.sh [OPTIONS]${NC}"

  echo -e "${YELLOW}Options:${NC}
    -h, --help              Show this help message
    -c, --clean             Clean build directory before building
    -r, --release           Build in Release mode (default: Debug)
    -p, --package           Package release artifacts
    -b DIR, --build-dir DIR Specify build directory (default: build)"

  echo -e "${YELLOW}Environment Variables:${NC}
    BUILD_TYPE   Set build type: Debug or Release (default: Debug)
    CLEAN_BUILD  Set to true to clean before building (default: false)"

  echo -e "${YELLOW}Examples:${NC}
    ./tools/build.sh                    # Debug build
    ./tools/build.sh --release          # Release build
    ./tools/build.sh --clean --release  # Clean Release build
    ./tools/build.sh --package          # Debug build + package
    BUILD_TYPE=Release ./tools/build.sh # Release build (env var)"
}

# Configuration
BUILD_DIR="build"
BUILD_TYPE="${BUILD_TYPE:-Debug}"
CLEAN_BUILD="${CLEAN_BUILD:-false}"
PACKAGE_RELEASE="${PACKAGE_RELEASE:-false}"

# Parse arguments
while [[ $# -gt 0 ]]; do
  case $1 in
    -h|--help)
      show_help
      exit 0
      ;;
    -c|--clean)
      CLEAN_BUILD=true
      shift
      ;;
    -r|--release)
      BUILD_TYPE="Release"
      shift
      ;;
    -p|--package)
      PACKAGE_RELEASE=true
      shift
      ;;
    -b|--build-dir)
      BUILD_DIR="$2"
      shift 2
      ;;
    *)
      echo -e "${RED}Unknown option: $1${NC}"
      show_help
      exit 1
      ;;
  esac
done

# Helper functions
log_info() {
  echo -e "${BLUE}ℹ️  $*${NC}"
}

log_success() {
  echo -e "${GREEN}✅ $*${NC}"
}

log_warning() {
  echo -e "${YELLOW}⚠️  $*${NC}"
}

log_error() {
  echo -e "${RED}❌ $*${NC}"
}

# Extract version from CMakeLists.txt
get_version() {
  grep -oP 'project\(\s*[\w-]+\s+VERSION\s+\K[0-9]+\.[0-9]+\.[0-9]+' CMakeLists.txt 2>/dev/null || echo "0.0.0"
}

# Main build script
main() {
  log_info "Starting build process..."
  log_info "Build Type: ${BUILD_TYPE}"
  log_info "Build Directory: ${BUILD_DIR}"

  # Get version
  local VERSION
  VERSION=$(get_version)
  local BUILD_NUMBER
  BUILD_NUMBER="$(date +%s)"
  local ARTIFACT_VERSION
  ARTIFACT_VERSION="${VERSION}-build-${BUILD_NUMBER}"

  log_info "Version: ${VERSION}, Build: ${BUILD_NUMBER}"

  # Clean build directory if requested
  if [ "$CLEAN_BUILD" = true ]; then
    log_info "Cleaning build directory..."
    rm -rf "$BUILD_DIR"
  fi

  # Create build directory
  mkdir -p "$BUILD_DIR"

  log_info "Installing Conan dependencies..."
  if conan install . --build=missing -s build_type="$BUILD_TYPE"; then
    log_success "Dependencies installed"
  else
    log_error "Failed to install dependencies"
    exit 1
  fi

  log_info "Configuring CMake (${BUILD_TYPE})..."
  if cmake -S . -B "$BUILD_DIR"/"$BUILD_TYPE" -DCMAKE_TOOLCHAIN_FILE="$PWD"/"$BUILD_DIR"/"$BUILD_TYPE"/generators/conan_toolchain.cmake -DCMAKE_BUILD_TYPE="$BUILD_TYPE" -DCMAKE_C_COMPILER=gcc-13 -DCMAKE_CXX_COMPILER=g++-13; then
    log_success "CMake configured"
  else
    log_error "CMake configuration failed"
    exit 1
  fi

  log_info "Building project..."
  if cmake --build "$BUILD_DIR/$BUILD_TYPE" --config "$BUILD_TYPE"; then
    log_success "Build completed"
  else
    log_error "Build failed"
    exit 1
  fi

  log_info "Running tests..."
  if ctest --test-dir "$BUILD_DIR/$BUILD_TYPE" --output-on-failure; then
    log_success "All tests passed"
  else
    log_warning "Some tests failed (continuing)"
  fi

  local BIN_NAME
  BIN_NAME=cpp-workspace-app

  if [ -f "$BUILD_DIR/$BUILD_TYPE/bin/$BIN_NAME" ]; then
    log_info "Running executable..."
    "$BUILD_DIR/$BUILD_TYPE/bin/$BIN_NAME"
    log_success "Executable ran successfully"
  else
    log_warning "Executable not found at $BUILD_DIR/bin/$BIN_NAME"
  fi

  local RELEASE_DIR
  RELEASE_DIR="$BUILD_DIR/release"

  mkdir -p "$RELEASE_DIR"
  cat > "$RELEASE_DIR/BUILD_INFO.txt" << EOF
Version: ${VERSION}
Build Number: ${BUILD_NUMBER}
Build Type: ${BUILD_TYPE}
Build Time: $(date -u +'%Y-%m-%dT%H:%M:%SZ')
CMake Version: $(cmake --version | head -n1)
Conan Version: $(conan --version 2>/dev/null || echo "unknown")
Build Directory: $(pwd)/$BUILD_DIR
EOF
  log_success "Build info generated: $RELEASE_DIR/BUILD_INFO.txt"

  if [ "$PACKAGE_RELEASE" = true ]; then
    log_info "Packaging release artifacts..."

    mkdir -p "$RELEASE_DIR"

    # Copy executable
    if [ -f "$BUILD_DIR/$BUILD_TYPE/bin/$BIN_NAME" ]; then
      cp "$BUILD_DIR/$BUILD_TYPE/bin/$BIN_NAME" "$RELEASE_DIR/"
    fi

    # Copy documentation
    [ -f "README.md" ] && cp README.md "$RELEASE_DIR/"
    [ -f "LICENSE" ] && cp LICENSE "$RELEASE_DIR/"
    [ -f "CHANGELOG.md" ] && cp CHANGELOG.md "$RELEASE_DIR/"

    # Create archives
    ARCHIVE_DIR=$(pwd)
    cd "$BUILD_DIR"
    tar -czf "../release-${ARTIFACT_VERSION}.tar.gz" release/
    zip -r "../release-${ARTIFACT_VERSION}.zip" release/ > /dev/null
    cd "$ARCHIVE_DIR"

    log_success "Release artifacts created:"
    ls -lh "release-${ARTIFACT_VERSION}".{tar.gz,zip}
  fi

  log_success "Build process completed successfully!"
  echo ""
  echo -e "${BLUE}Build Summary:${NC}"
  echo "  Version: ${VERSION}"
  echo "  Build Number: ${BUILD_NUMBER}"
  echo "  Build Type: ${BUILD_TYPE}"
  echo "  Build Directory: ${BUILD_DIR}"
  [ "$PACKAGE_RELEASE" = true ] && echo "  Artifacts: release-${ARTIFACT_VERSION}.{tar.gz,zip}"
  echo ""
}

# Run main function
main "$@"
