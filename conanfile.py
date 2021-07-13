from conans import ConanFile, CMake, tools

class CppAgentConan(ConanFile):
    name = "mtconnect_cppagent"
    version = "1.7"
    generators = "cmake"
    url = "https://github.com/mtconnect/cppagent_dev.git"
    license = "Apache License 2.0"
    settings = "os", "compiler", "arch", "build_type", "arch_build"
    options = { "run_tests": [True, False] }
    description = "MTConnect reference C++ agent copyright Association for Manufacturing Technology"
    
    requires = ["boost/1.75.0", "libxml2/2.9.10", "date/2.4.1", "nlohmann_json/3.9.1", 
    	        "mqtt_cpp/9.0.0", "openssl/1.1.1k"]
    build_policy = "missing"
    default_options = {
        "run_tests": True,

        "boost:python_version": "3.9",
        "boost:python_executable": "python3",
        "boost:shared": False,
        "boost:bzip2": False,
        "boost:lzma": False,
        "boost:without_python": False,
        "boost:without_wave": True,
        "boost:without_test": True,
        "boost:without_json": True,
        "boost:without_mpi": True,
        "boost:without_stacktrace": True,
        "boost:extra_b2_flags": "-j 2 -d +1 cxxstd=17 ",
        "boost:i18n_backend_iconv": 'off',
        "boost:i18n_backend_icu": True,

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
        self.windows_xp = self.settings.os == 'Windows' and self.settings.compiler.toolset and \
                          self.settings.compiler.toolset in ('v141_xp', 'v140_xp')
        if self.settings.os == 'Windows':
            self.options['boost'].i18n_backend_icu = False
            self.options["boost"].extra_b2_flags = self.options["boost"].extra_b2_flags + \
                " boost.locale.icu=off boost.locale.iconv=off boost.locale.winapi=on asmflags=/safeseh "
            if self.settings.build_type and self.settings.build_type == 'Debug':
                self.settings.compiler.runtime = 'MTd'
            else:
                self.settings.compiler.runtime = 'MT'
            self.settings.compiler.version = '16'
        
        if "libcxx" in self.settings.compiler.fields and self.settings.compiler.libcxx == "libstdc++":
            raise Exception("This package is only compatible with libstdc++11, add -s compiler.libcxx=libstdc++11")
        
        self.settings.compiler.cppstd = 17
        
        if self.windows_xp:
            self.options["boost"].extra_b2_flags = self.options["boost"].extra_b2_flags + "define=BOOST_USE_WINAPI_VERSION=0x0501 "
        elif self.settings.os == 'Windows':
            self.options["boost"].extra_b2_flags = self.options["boost"].extra_b2_flags + "define=BOOST_USE_WINAPI_VERSION=0x0600 "            

    def requirements(self):
        if not self.windows_xp:
            self.requires("gtest/1.10.0")
        
    def build(self):
        cmake = CMake(self)
        cmake.verbose = True
        if self.windows_xp:
            cmake.definitions['AGENT_ENABLE_UNITTESTS'] = 'OFF'
            cmake.definitions['WINVER'] = '0x0501'
        else:
            flags = False

        cmake.configure()
        cmake.build()
        if not self.windows_xp and self.options.run_tests:
            cmake.test()

    def imports(self):
        self.copy("*.dll", "bin", "bin")
        self.copy("*.so*", "bin", "lib")
        self.copy("*.dylib", "bin", "lib")

        
    
    
