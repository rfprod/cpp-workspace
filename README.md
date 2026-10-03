# cpp-workspace

C++ workspace template with workflow automation.

## Requirements

In order to run your own copy of the project one must fulfill the following requirements.

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
- C/C++ Extension Pack (Microsoft)
- CMake Tools (Microsoft)
- CMake (twxs)

🆗 **Visual Studio** - for Windows development

### Core dependencies

- [Git]((https://git-scm.com/))
- [Python](https://www.python.org/) or [NodeJS](https://nodejs.org/) `for conventional commits`
- [Bash 5](https://www.gnu.org/software/bash/) `for scripting`
- [CMake](https://cmake.org/)
- A **Build system** (one of):
  - [Make](https://en.wikipedia.org/wiki/Make_(software))
  - [Ninja](https://ninja-build.org/)
- A **C++ compiler** (one of):
  - [GCC](https://gcc.gnu.org/)
  - [Clang](https://clang.llvm.org/)
  - [MSVC](https://visualstudio.microsoft.com/vs/features/cplusplus/)

## Committing changes to the repo

### Linux

Using [commitizen cli](https://github.com/commitizen/cz-cli) is mandatory.

The commit message are validated during the premerge checks.

It is assumed that [Node.js](https://nodejs.org/) is installed.

Given the [NodeJS](https://nodejs.org/) is installed, and [commitizen cli is installed as a global dependency](https://github.com/commitizen/cz-cli#conventional-commit-messages-as-a-global-utility), the following command should be used to initiate the commit process

```bash
git cz
```

Alternatively, given there are no conflicts with other projects that use [the commitizen npm package](https://www.npmjs.com/package/commitizen), one could install commitizen globally via `pypi` like this

```bash
sudo pip3 install -U Commitizen
```

### OSX

Using [commitizen](https://pypi.org/project/commitizen/) is mandatory.

The commit message are validated during the premerge checks.

After installing the package as a global utility using the following command

```bash
brew install commitizen
```

one can use one of the following commands to initiate the commit process

```bash
cz commit
```

or

```bash
cz c
```

## FAQ

### Manual build

```bash
rm -rf ./build/*
conan install . --build=missing
cmake --preset conan-release
cmake --build --preset conan-release
```

or

```bash
rm -rf ./build/* ./CMakeUserPresets.json
conan install . --build=missing
cmake --preset conan-release
cmake --build --preset conan-release
```
