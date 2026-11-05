from conan import ConanFile
from conan.tools.cmake import cmake_layout


class AppConan(ConanFile):
    settings = "os", "compiler", "build_type", "arch"

    options = {"with_tests": [True, False]}

    default_options = {
        "with_tests": False,
        ##
        # If ncurses++ is linked (check using `grep -c "ncurses++" build/Release/tests/CMakeFiles/cpp-workspace-tests.dir/link.txt`),
        # unit tests will fail if `GTest::gtest_main` is linked after `cpp-workspace-core` in `./tests/CMakeLists.txt`.
        ##
        "ncurses/*:with_cxx": False,
    }

    generators = "CMakeDeps", "CMakeToolchain"

    def requirements(self):
        if self.requires != None:
            self.requires("fmt/11.2.0")
            self.requires("libcurl/8.5.0")
            self.requires("nlohmann_json/3.11.2")
            self.requires("ncurses/6.5")
        if getattr(self.options, "with_tests") == True and self.test_requires != None:
            self.test_requires("gtest/1.15.0")

    def layout(self):
        cmake_layout(self)
