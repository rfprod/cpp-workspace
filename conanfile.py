from conan import ConanFile
from conan.tools.cmake import cmake_layout


class AppConan(ConanFile):
    settings = "os", "compiler", "build_type", "arch"

    generators = "CMakeDeps", "CMakeToolchain"

    def requirements(self):
        self.requires("fmt/11.2.0")  # Replace with your dependencies

    def layout(self):
        cmake_layout(self)
