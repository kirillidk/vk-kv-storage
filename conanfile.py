from conan import ConanFile

class KVStorage(ConanFile):
    settings = "os", "compiler", "build_type", "arch"
    generators = "CMakeDeps", "CMakeToolchain"

    def requirements(self):
        self.requires("boost/1.88.0")
        self.requires("gtest/1.16.0")