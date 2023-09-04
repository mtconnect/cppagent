
import os

from conan import ConanFile
from conan.tools.cmake import CMake, CMakeToolchain, CMakeDeps, cmake_layout
from conan.tools.build import can_run


class MTConnectAgentTest(ConanFile):
    settings = "os", "compiler", "build_type", "arch"
    test_type = "explicit"
    
    def build_requirements(self):
        self.tool_requires("cmake/3.26.4")
        
    def requirements(self):
        self.requires(self.tested_reference_str)

    def generate(self):
        agent_options = self.dependencies[self.tested_reference_str].options
        
        tc = CMakeToolchain(self)

        if agent_options.shared:
            tc.cache_variables['SHARED_AGENT_LIB'] = True
        if agent_options.with_ruby:
            tc.cache_variables['WITH_RUBY'] = True
        
        tc.generate()
        
        deps = CMakeDeps(self)
        deps.generate()

    def layout(self):
        cmake_layout(self)

    def build(self):
        cmake = CMake(self)
        cmake.verbose = True
        cmake.configure(cli_args=['--log-level=DEBUG'])
        cmake.build()

        
    def test(self):
        if can_run(self):
            cmake = CMake(self)
            cmake.test()
        
