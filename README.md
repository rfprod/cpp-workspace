# cpp-workspace

A C++ Retrieval-Augmented Generation (RAG) application powered by Ollama, featuring embeddings via Qdrant vector database, ncurses UI with theme support, and streaming output.

## Capabilities

- Connects to local Ollama for LLM inference.
- Generates embeddings and stores them in Qdrant vector database.
- Retrieves relevant documents and generates context-aware answers.
- Provides an interactive ncurses terminal UI with streaming responses.

## Requirements

In order to run your own copy of the project it is recommended to satisfly the following requirements.

### Supported operating systems

- 🏆 **Debian based Linux**
  - see help for available options:
    ```bash
    # always inspect scripts before running them on your machine
    bash tools/shell/install.sh ?
    ```
  - install all dependencies required to work with the project:
    ```bash
    # always inspect scripts before running them on your machine
    bash tools/shell/install.sh all
    ```
- 🆗 **macOS**
  - see help for available options:
    ```bash
    # always inspect scripts before running them on your machine
    bash tools/shell/install.sh ?
    ```
  - install all dependencies required to work with the project (some dependencies may be missing):
    ```bash
    # always inspect scripts before running them on your machine
    bash tools/shell/install.sh all osx
    ```
- 🤷 **Windows**
  - Install using one of the following approaches:
    - **MSVC** (recommended): Download and install [Visual Studio Community](https://visualstudio.microsoft.com/community/) with C++ build tools
    - **MinGW**: Install via [MSYS2](https://www.msys2.org/) or [Chocolatey](https://chocolatey.org/)
    - **WSL2**: Use Windows Subsystem for Linux and follow the Linux instructions
    - Additionally, install [CMake](https://cmake.org/download/) and [Git for Windows](https://git-scm.com/download/win).
    - given that the dev environment is set up, the following commands should be used to install `shellcheck` via PowerShell;
      ```powershell
      iwr -useb get.scoop.sh | iex
      scoop install shellcheck
      ```
    - recommended shell: [Git for Windows](https://gitforwindows.org/) > `Git BASH`;
    - configure Git to use LF as a carriage return
      ```bash
      git config --global core.autocrlf false
      git config --global core.eol lf

### Integrated development environment

🏆 **Visual Studio Code** - recommended for all operating systems

Suggested extensions:
- [C/C++ Extension Pack](https://marketplace.visualstudio.com/items?itemName=ms-vscode.cpptools-extension-pack)
- [CMake Tools](https://marketplace.visualstudio.com/items?itemName=ms-vscode.cmake-tools)

See [`.vscode/extensions`](.vscode/extensions.json) for the complete list.

### Core dependencies

- [Git]((https://git-scm.com/))
- [Python](https://www.python.org/) or [NodeJS](https://nodejs.org/) _for conventional commits_
- [Bash 5](https://www.gnu.org/software/bash/) _for scripting_
- [CMake](https://cmake.org/)
- A **Build system** (one of):
  - [Make](https://en.wikipedia.org/wiki/Make_(software))
  - [Ninja](https://ninja-build.org/)
- A **C++ compiler** (one of):
  - [GCC](https://gcc.gnu.org/)
  - [Clang](https://clang.llvm.org/)
  - [MSVC](https://visualstudio.microsoft.com/vs/features/cplusplus/)
- [Docker](https://www.docker.com/) _to run [Qdrant](https://qdrant.tech/) locally_
- [Ollama](https://ollama.com/)

### Environment variables

In the project root, create a `.env` file with the following content

```env
export QDRANT_URL="http://localhost:6333"
export QDRANT_API_KEY="your-secure-read-and-write-key"
export QDRANT_API_KEY_READ_ONLY="your-secure-read-only-key"
```

## Contributing: Conventional Commits

This project requires [Commitizen](https://commitizen.org/) for standardized commit messages.

**Setup:**
- **Linux/macOS:** `brew install commitizen` or `pip install commitizen`
- **Windows:** Use WSL2 and follow Linux instructions, or install via [MSYS2](https://www.msys2.org/)

**Committing:**
```bash
cz commit  # Interactive commit prompt
```

## FAQ

### Commands

1. Install

    ```bash
    bash ./tools/shell/install.sh --help
    ```

2. Build

    ```bash
    bash ./tools/shell/build.sh --help
    ```

3. Format

    ```bash
    bash ./tools/shell/format.sh
    ```

### Manual build

```bash
rm -rf ./build/* ;\
  conan install . --build=missing ;\
  cmake --preset conan-release ;\
  cmake --build --preset conan-release
```

or

```bash
rm -rf ./build/* ./CMakeUserPresets.json ;\
  conan install . --build=missing ;\
  cmake --preset conan-release ;\
  cmake --build --preset conan-release
```

### Run the app

Source .env first, then run the app

```bash
source .env; \
  ./build/Release/bin/cpp-workspace-app
```

### Quick Start

1. Install Ollama and pull two models, e.g. [__embeddinggemma:latest__](https://ollama.com/library/embeddinggemma) and [__gemma3n:latest__](https://ollama.com/library/gemma3n:latest).
2. Build the project or [download the latest release](https://github.com/rfprod/cpp-workspace/releases):
    ```bash
    bash tools/shell/build.sh --clean --release --exe-args '--skip_embedding=true'
    ```
3. Run in interactive RAG mode:
    ```bash
    ./build/Release/bin/cpp-workspace-app --interactive=true --rag=true
    ```
