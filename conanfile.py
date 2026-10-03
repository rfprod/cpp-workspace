from conan import ConanFile
from conan.tools.cmake import cmake_layout


class AppConan(ConanFile):
    settings = "os", "compiler", "build_type", "arch"

    generators = "CMakeDeps", "CMakeToolchain"

    def requirements(self):
        self.requires("fmt/11.2.0")
        self.requires("libcurl/8.5.0")
        self.requires("nlohmann_json/3.11.2")

    def layout(self):
        cmake_layout(self)
