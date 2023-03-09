from conans import ConanFile, CMake, tools
from conans.errors import ConanInvalidConfiguration
import os
import io
import re
import itertools as it
import glob
import subprocess
from conan.tools.microsoft import is_msvc, is_msvc_static_runtime

class MTConnectAgentConan(ConanFile):
    name = "mtconnect_agent"
    version = "2.1"
    generators = "cmake"
    url = "https://github.com/mtconnect/cppagent.git"
    license = "Apache License 2.0"
    settings = "os", "compiler", "arch", "build_type", "arch_build"
    options = { "run_tests": [True, False], "build_tests": [True, False], 
                "without_ipv6": [True, False], "with_ruby": [True, False],
                 "development" : [True, False], "shared": [True, False], "winver": "ANY",
                 "with_docs" : [True, False] }
    description = "MTConnect reference C++ agent copyright Association for Manufacturing Technology"
    
    requires = ["boost/1.79.0@#3249d9bd2b863a9489767bf9c8a05b4b",
                "libxml2/2.9.10@#9133e645e934381d3cc4f6a0bf563fbe",
                "date/2.4.1@#178e4ada4fefd011aaa81ab2bca646db",
                "nlohmann_json/3.9.1@#a41bc0deaf7f40e7b97e548359ccf14d", 
                "openssl/3.0.5@#40f4488f02b36c1193b68f585131e8ef",
                "mqtt_cpp/13.1.0"]

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

        "boost:shared": False,
        "boost:without_python": True,
        "boost:without_test": True,

        "libxml2:shared": False,
        "libxml2:include_utils": False,
        "libxml2:http": False,
        "libxml2:ftp": False,
        "libxml2:iconv": False,
        "libxml2:zlib": False,

        "gtest:shared": False,

        "openssl:shared": False,
        
        "date:use_system_tz_db": True
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
           str(self.settings.compiler.runtime).startswith('MT'):
            raise ConanInvalidConfiguration("Shared can only be built with DLL runtime.")

    def configure(self):
        self.windows_xp = self.settings.os == 'Windows' and self.settings.compiler.toolset and \
                          self.settings.compiler.toolset in ('v141_xp', 'v140_xp')
        if self.settings.os == 'Windows':
            if self.windows_xp:
                self.options.build_tests = False
                self.options.winver = '0x501'
                
            if self.options.shared:
                if self.settings.build_type and self.settings.build_type == 'Debug':
                    self.settings.compiler.runtime = 'MDd'
                else:
                    self.settings.compiler.runtime = 'MD'
            else:
                if self.settings.build_type and self.settings.build_type == 'Debug':
                    self.settings.compiler.runtime = 'MTd'
                else:
                    self.settings.compiler.runtime = 'MT'
                                
            if not self.settings.compiler.version:
                self.settings.compiler.version = '16'
        
        if "libcxx" in self.settings.compiler.fields and self.settings.compiler.libcxx == "libstdc++":
            raise Exception("This package is only compatible with libstdc++11, add -s compiler.libcxx=libstdc++11")
        
        self.settings.compiler.cppstd = 17


        if not self.options.build_tests:
            self.options.run_tests = False

        if not self.options.shared and self.settings.os == "Macos":
            self.options["boost"].visibility = "hidden"

        # Make sure shared builds use shared boost
        if is_msvc(self) and self.options.shared:
            self.options["boost"].shared = True
            self.options["libxml2"].shared = True
            self.options["gtest"].shared = True
            self.options["openssl"].shared = True
        
        self.run("conan export conan/mqtt_cpp", cwd=os.path.dirname(__file__))
        if self.options.with_ruby:
          self.run("conan export conan/mruby", cwd=os.path.dirname(__file__))

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
        if not self.options.build_tests:
            cmake.definitions['AGENT_ENABLE_UNITTESTS'] = 'OFF'

        if self.options.without_ipv6:
            cmake.definitions['AGENT_WITHOUT_IPV6'] = 'ON'

        if self.options.with_ruby:
            cmake.definitions['WITH_RUBY'] = 'ON'
        else:
            cmake.definitions['WITH_RUBY'] = 'OFF'

        if self.settings.os == 'Windows':
            cmake.definitions['WINVER'] = self.options.winver

        if self.options.shared:
            cmake.definitions['SHARED_AGENT_LIB'] = 'ON'

        cmake.configure()
        cmake.build()
        if self.options.run_tests:
            cmake.test()

    def imports(self):
        self.copy("*.dll", "bin", "bin")
        self.copy("*.pdb", "bin", "bin", keep_path=False)
        self.copy("*.pdb", "lib", "bin", keep_path=False)
        self.copy("*.so*", "lib", "lib")
        self.copy("*.dylib", "lib", "lib")

    def package_info(self):
        self.cpp_info.includedirs = ['include']
        self.cpp_info.libs = ['agent_lib']

        self.cpp_info.defines = []
        if self.options.with_ruby:
            self.cpp_info.defines.append("WITH_RUBY=1")
        if self.options.without_ipv6:
            self.cpp_info.defines.append("AGENT_WITHOUT_IPV6=1")
        if self.options.shared:
            self.user_info.SHARED = 'ON'
            self.cpp_info.defines.append("SHARED_AGENT_LIB=1")
            self.cpp_info.defines.append("BOOST_ALL_DYN_LINK")
            
        if self.settings.os == 'Windows':
            winver=str(self.options.winver)
            self.cpp_info.defines.append("WINVER=" + winver)
            self.cpp_info.defines.append("_WIN32_WINNT=" + winver)

    def package(self):
        self.copy("*", src=os.path.join(self.build_folder, "bin"), dst="bin", keep_path=False)
        self.copy("*.a", src=os.path.join(self.build_folder, "lib"), dst="lib", keep_path=False)
        self.copy("*.lib", src=os.path.join(self.build_folder, "lib"), dst="lib", keep_path=False)
        self.copy("*.dylib", src=os.path.join(self.build_folder, "lib"), dst="lib", keep_path=False)
        self.copy("*.so", src=os.path.join(self.build_folder, "lib"), dst="lib", keep_path=False)
        self.copy("*.h", src=os.path.join(self.build_folder, "agent_lib"), dst="include")
        self.copy("*.h", src="src", dst="include")
        self.copy("*.hpp", src="src", dst="include")

    
