
import os

from conan import ConanFile
from conan.tools.cmake import CMake, CMakeToolchain, CMakeDeps, cmake_layout
from conan.tools.build import can_run


class MTConnectAgentTest(ConanFile):
    settings = "os", "compiler", "build_type", "arch"

    requires = ["boost/1.79.0",
                "libxml2/2.10.3",
                "date/2.4.1",
                "nlohmann_json/3.9.1",
                "openssl/3.0.8",
                "rapidjson/cci.20220822",
                "mqtt_cpp/13.1.0",
                "bzip2/1.0.8",
                
                ]

    default_options = {
        "mtconnect_agent/*:without_ipv6": False,
        "mtconnect_agent/*:with_ruby": True,
        "mtconnect_agent/*:development": False,
        "mtconnect_agent/*:shared": True,
        "mtconnect_agent/*:winver": "0x600",
        "mtconnect_agent/*:with_docs": False,
        "mtconnect_agent/*:cpack": False,
        "mtconnect_agent/*:agent_prefix": None,

        "boost*:shared": False,
        "boost*:without_python": True,
        "boost*:without_test": True,
        "boost*:without_graph": True,
        "boost*:without_test": True,
        "boost*:without_nowide": True,
        "boost*:without_fiber": True,
        "boost*:without_math": True,
        "boost*:without_contract": True,
        "boost*:without_serialization": True,
        "boost*:without_wave": True,
        "boost*:without_graph_parallel": True,

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

    def configure(self):
        self.options["boost/*"].shared = True

    def requirements(self):
        self.requires("mruby/3.2.0")
        self.requires("gtest/1.10.0")
        self.requires(self.tested_reference_str)

    def generate(self):
        tc = CMakeToolchain(self)

        tc.cache_variables['SHARED_AGENT_LIB'] = True
        tc.cache_variables['WITH_RUBY'] = True
        tc.generate()
        deps = CMakeDeps(self)
        deps.generate()

    def layout(self):
        cmake_layout(self)

    def build(self):
        cmake = CMake(self)
        cmake.configure()
        cmake.build()

        
    def test(self):
        if can_run(self):
            cmake = CMake(self)
            cmake.test()
        
