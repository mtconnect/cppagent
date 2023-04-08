from conan import ConanFile
from conan.tools.microsoft import is_msvc, is_msvc_static_runtime
from conan.tools.cmake import CMake, CMakeToolchain, CMakeDeps, cmake_layout
from conan.tools.files import copy

import os
import io
import re
import itertools as it
import glob
import subprocess

class MTConnectAgentConan(ConanFile):
    name = "mtconnect_agent"
    version = "2.2"
    url = "https://github.com/mtconnect/cppagent.git"
    license = "Apache License 2.0"
    settings = "os", "compiler", "arch", "build_type"
    options = { "run_tests": [True, False], "build_tests": [True, False], 
                "without_ipv6": [True, False], "with_ruby": [True, False],
                 "development" : [True, False], "shared": [True, False], "winver": [None, "ANY"],
                 "with_docs" : [True, False] }
    description = "MTConnect reference C++ agent copyright Association for Manufacturing Technology"
    
    requires = ["boost/1.79.0",
                "libxml2/2.10.3",
                "date/2.4.1",
                "nlohmann_json/3.9.1",
                "openssl/3.0.8",
                "mqtt_cpp/13.1.0",
                "rapidjson/cci.20220822"]

    build_requires = ["cmake/[>3.23.0]"]
    
    build_policy = "missing"
    default_options = {
        "run_tests": True,
        "build_tests": True,
        "without_ipv6": False,
        "with_ruby": True,
        "development": False,
        "shared": False,
        "winver": "0x600",
        "with_docs": False,

        "boost*:shared": False,
        "boost*:without_python": True,
        "boost*:without_test": True,

        "libxml2*:shared": False,
        "libxml2*:include_utils": False,
        "libxml2*:http": False,
        "libxml2*:ftp": False,
        "libxml2*:iconv": False,
        "libxml2*:zlib": False,

        "gtest*:shared": False,

        "openssl*:shared": False,
        
        "date*:use_system_tz_db": True
        }

    exports = "conan/*"
    exports_sources = "*"

#    def source(self):
#        git = tools.Git(self.source_folder)
#        if self.options.development:
#            git.clone("git@github.com:/mtconnect/cppagent_dev")
#        else:
#            git.clone("https://github.com/mtconnect/cppagent")

    def validate(self):
        if self.settings.os == 'Windows' and self.options.shared and \
           self.settings.compiler.runtime != 'dynamic':
            raise ConanInvalidConfiguration("Shared can only be built with DLL runtime.")

    def layout(self):
        self.folders.build_folder_vars = ["options.shared"]
        cmake_layout(self)

    def configure(self):
        self.windows_xp = self.settings.os == 'Windows' and self.settings.compiler.toolset and \
                          self.settings.compiler.toolset in ('v141_xp', 'v140_xp')
        if self.settings.os == 'Windows':
            if self.windows_xp:
                self.options.build_tests._value = False
                self.options.winver = '0x501'
                
            if not self.settings.compiler.version:
                self.settings.compiler.version = '16'
        
        if "libcxx" in self.settings.compiler.fields and self.settings.compiler.libcxx == "libstdc++":
            raise Exception("This package is only compatible with libstdc++11, add -s compiler.libcxx=libstdc++11")
        
        self.settings.compiler.cppstd = 17

        if not self.options.build_tests:
            self.options.run_tests._value = False

        if not self.options.shared and self.settings.os == "Macos":
            self.options["boost"].visibility = "hidden"

        # Make sure shared builds use shared boost
        print("**** Checking if it is shared")
        if is_msvc(self) and self.options.shared:
            print("**** Making boost, libxml2, gtest, and openssl shared")
            self.options["bzip2/*"].shared = True
            self.options["boost/*"].shared = True
            self.options["libxml2/*"].shared = True
            self.options["gtest/*"].shared = True
            self.options["openssl/*"].shared = True

    def generate(self):
        if self.options.shared:
            for dep in self.dependencies.values():
                if dep.cpp_info.bindirs:
                    print("Copying from " + dep.cpp_info.bindirs[0] + " to " + os.path.join(self.build_folder, "dlls"))
                    copy(self, "*.dll", dep.cpp_info.bindirs[0], os.path.join(self.build_folder, "dlls"))
        
        tc = CMakeToolchain(self)

        tc.cache_variables['SHARED_AGENT_LIB'] = self.options.shared.__bool__()
        tc.cache_variables['WITH_RUBY'] = self.options.with_ruby.__bool__()
        tc.cache_variables['AGENT_WITH_DOCS'] = self.options.with_docs.__bool__()
        tc.cache_variables['AGENT_ENABLE_UNITTESTS'] = self.options.build_tests.__bool__()
        tc.cache_variables['AGENT_WITHOUT_IPV6'] = self.options.without_ipv6.__bool__()
        if self.settings.os == 'Windows':
            tc.cache_variables['WINVER'] = self.options.winver

        tc.generate()
        deps = CMakeDeps(self)
        deps.generate()
        
    def build_requirements(self):
        if self.options.with_docs:
            res = subprocess.run(["doxygen --version"], shell=True, text=True, capture_output=True)
            if (res.returncode != 0 or not res.stdout.startswith('1.9')):
                self.tool_requires("doxygen/1.9.4@#19fe2ac34109f3119190869a4d0ffbcb")

    def requirements(self):
        if not self.windows_xp:
            self.requires("gtest/1.10.0")
        if self.options.with_ruby:
            self.requires("mruby/3.1.0")
        
    def build(self):
        cmake = CMake(self)
        cmake.verbose = True
        cmake.configure(cli_args=['--log-level=DEBUG'])
        cmake.build()
        if self.options.with_docs:
            cmake.build(build_type=None, target='docs')
        if self.options.run_tests and self.options.build_tests:
            cmake.test()

    def package_info(self):
        self.cpp_info.includedirs = ['include']
        self.cpp_info.libdirs = ['lib']
        self.cpp_info.bindirs = ['bin']
        self.cpp_info.libs = ['agent_lib']

        self.cpp_info.defines = []
        if self.options.with_ruby:
            self.cpp_info.defines.append("WITH_RUBY=1")
        if self.options.without_ipv6:
            self.cpp_info.defines.append("AGENT_WITHOUT_IPV6=1")
        if self.options.shared:
            self.cpp_info.defines.append("SHARED_AGENT_LIB=1")
            self.cpp_info.defines.append("BOOST_ALL_DYN_LINK")
            
        if self.settings.os == 'Windows':
            winver=str(self.options.winver)
            self.cpp_info.defines.append("WINVER=" + winver)
            self.cpp_info.defines.append("_WIN32_WINNT=" + winver)

    def package(self):
        cmake = CMake(self)
        cmake.install()


    
