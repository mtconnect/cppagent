from conans import ConanFile, CMake, tools

class CppAgentConan(ConanFile):
    name = "mtconnect_cppagent"
    version = "2.0"
    generators = "cmake"
    url = "https://github.com/mtconnect/cppagent.git"
    license = "Apache License 2.0"
    settings = "os", "compiler", "arch", "build_type", "arch_build"
    options = { "run_tests": [True, False], "build_tests": [True, False], "without_python": [True, False],
               "without_ruby": [True, False], "without_ipv6": [True, False], "with_ruby": [True, False],
               "with_python": [True, False] }
    description = "MTConnect reference C++ agent copyright Association for Manufacturing Technology"
    
    requires = ["boost/1.77.0", "libxml2/2.9.10", "date/2.4.1", "nlohmann_json/3.9.1", 
    	        "mqtt_cpp/11.0.0", "openssl/1.1.1k"]
    build_policy = "missing"
    default_options = {
        "run_tests": True,
        "build_tests": True,
        "without_python": True,
        "without_ruby": False,
        "without_ipv6": False,
        "with_python": False,
        "with_ruby": False,

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
        
        "date:use_system_tz_db": True
        }

    def configure(self):
        if not self.options.without_python:
            self.options["boost"].without_python = False
            
        self.windows_xp = self.settings.os == 'Windows' and self.settings.compiler.toolset and \
                          self.settings.compiler.toolset in ('v141_xp', 'v140_xp')
        if self.settings.os == 'Windows':
            if self.settings.build_type and self.settings.build_type == 'Debug':
                self.settings.compiler.runtime = 'MTd'
            else:
                self.settings.compiler.runtime = 'MT'
            self.settings.compiler.version = '16'
        
        if "libcxx" in self.settings.compiler.fields and self.settings.compiler.libcxx == "libstdc++":
            raise Exception("This package is only compatible with libstdc++11, add -s compiler.libcxx=libstdc++11")
        
        self.settings.compiler.cppstd = 17

        if self.windows_xp:
            self.options.build_tests = False

        if not self.options.build_tests:
            self.options.run_tests = False

        if not self.options.without_ruby:
            self.options.with_ruby = True
            
        if not self.options.without_python:
            self.options.with_python = True

        if self.settings.os == "Macos":
            self.options["boost"].visibility = "hidden"
        
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

        if self.options.with_python:
            cmake.definitions['WITH_PYTHON'] = 'ON'
        else:
            cmake.definitions['WITH_PYTHON'] = 'OFF'
            
        if self.options.with_ruby:
            cmake.definitions['WITH_RUBY'] = 'ON'
        else:
            cmake.definitions['WITH_RUBY'] = 'OFF'
            
        if self.windows_xp:
            cmake.definitions['WINVER'] = '0x0501'

        cmake.configure()
        cmake.build()
        if self.options.run_tests:
            cmake.test()

    def imports(self):
        self.copy("*.dll", "bin", "bin")
        self.copy("*.so*", "bin", "lib")
        self.copy("*.dylib", "bin", "lib")

        
    
    
