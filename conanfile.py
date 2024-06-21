from conan import ConanFile
from conan.tools.microsoft import is_msvc, is_msvc_static_runtime
from conan.tools.cmake import CMake, CMakeToolchain, CMakeDeps, cmake_layout
from conan.tools.files import copy

import os
import io
import re

class MTConnectAgentConan(ConanFile):
    name = "mtconnect_agent"
        self.requires("boost/1.85.0", headers=True, libs=True, transitive_headers=True, transitive_libs=True)
        self.requires("libxml2/2.10.3", headers=True, libs=True, visible=True, transitive_headers=True, transitive_libs=True)
        self.requires("date/2.4.1", headers=True, libs=True, transitive_headers=True, transitive_libs=True)
        self.requires("nlohmann_json/3.9.1", headers=True, libs=False, transitive_headers=True, transitive_libs=False)
        self.requires("openssl/3.0.8", headers=True, libs=True, transitive_headers=True, transitive_libs=True)
        self.requires("rapidjson/cci.20220822", headers=True, libs=False, transitive_headers=True, transitive_libs=False)
        self.requires("mqtt_cpp/13.2.1", headers=True, libs=False, transitive_headers=True, transitive_libs=False)
        self.requires("bzip2/1.0.8", headers=True, libs=True, transitive_headers=True, transitive_libs=True)
        
        if self.options.with_ruby:
            self.requires("mruby/3.2.0", headers=True, libs=True, transitive_headers=True, transitive_libs=True)

        self.requires("gtest/1.10.0", headers=True, libs=True, transitive_headers=True, transitive_libs=True, test=True)
        
    def configure(self):
        if self.options.shared:
            self.options.rm_safe("fPIC")

        if self.options.shared:
            self.options["boost/*"].shared = True
            self.package_type = "shared-library"

        if is_msvc(self):
            self.options["boost/*"].extra_b2_flags = ("define=BOOST_USE_WINAPI_VERSION=" + str(self.options.winver))
            
        # Make sure shared builds use shared boost
        if is_msvc(self) and self.options.shared:
            print("**** Making boost, libxml2, gtest, and openssl shared")
            self.options["bzip2/*"].shared = True
            self.options["libxml2/*"].shared = True
            self.options["gtest/*"].shared = True
            self.options["openssl/*"].shared = True
        
        self.run("conan export conan/mqtt_cpp", cwd=os.path.dirname(__file__))
        if self.options.with_ruby:
            self.run("conan export conan/mruby", cwd=os.path.dirname(__file__))

        if not self.options.cpack_generator:
            if is_msvc(self):
                self.options.cpack_generator = "ZIP"
            else:
                self.options.cpack_generator = "TGZ"                

    def generate(self):
        if self.options.shared:
            for dep in self.dependencies.values():
                if dep.cpp_info.bindirs:
                    if is_msvc(self) and dep.cpp_info.bindirs:
                        print("Copying from " + dep.cpp_info.bindirs[0] + " to " + os.path.join(self.build_folder, "libs"))
                        copy(self, "*.dll", dep.cpp_info.bindirs[0], os.path.join(self.build_folder, "libs"), keep_path=False)
                    elif dep.cpp_info.libdirs:
                        print("Copying from " + dep.cpp_info.libdirs[0] + " to " + os.path.join(self.build_folder, "libs"))
                        # copy(self, "*.a", dep.cpp_info.libdirs[0], os.path.join(self.build_folder, "libs"), keep_path=False)
                        copy(self, "*.so*", dep.cpp_info.libdirs[0], os.path.join(self.build_folder, "libs"), keep_path=False)
                        copy(self, "*.dylib", dep.cpp_info.libdirs[0], os.path.join(self.build_folder, "libs"), keep_path=False)            
        
        tc = CMakeToolchain(self)
        tc.cache_variables['SHARED_AGENT_LIB'] = self.options.shared.__bool__()
        tc.cache_variables['WITH_RUBY'] = self.options.with_ruby.__bool__()
        tc.cache_variables['AGENT_WITH_DOCS'] = self.options.with_docs.__bool__()
        tc.cache_variables['AGENT_WITHOUT_IPV6'] = self.options.without_ipv6.__bool__()
        tc.cache_variables['DEVELOPMENT'] = self.options.development.__bool__()
        if self.options.agent_prefix:
            tc.cache_variables['AGENT_PREFIX'] = self.options.agent_prefix
        if is_msvc(self):
            tc.cache_variables['WINVER'] = self.options.winver

        tc.generate()
        
        deps = CMakeDeps(self)
        deps.generate()
        
    def build(self):
        cmake = CMake(self)
        cmake.verbose = True
        cmake.configure(cli_args=['--log-level=DEBUG'])
        cmake.build()
        if self.options.with_docs:
            cmake.build(build_type=None, target='docs')

        if self.options.development and not self.conf.get("tools.build:skip_test", default=False):
            cmake.test(env='CTEST_OUTPUT_ON_FAILURE=YES')

    def package_info(self):
        self.cpp_info.includedirs = ['include']
        self.cpp_info.libdirs = ['lib']
        self.cpp_info.bindirs = ['bin']
        output_name = 'agent_lib'
        if self.options.agent_prefix:
            output_name = str(self.options.agent_prefix) + output_name
        self.cpp_info.libs = [output_name]

        self.cpp_info.defines = ['MQTT_USE_TLS=ON',
                                 'MQTT_USE_WS=ON',
                                 'MQTT_USE_STR_CHECK=ON',
                                 'MQTT_STD_VARIANT',
                                 'MQTT_STD_OPTIONAL',
                                 'MQTT_STD_STRING_VIEW',
                                 'MQTT_USE_LOG',
                                 'BOOST_FILESYSTEM_VERSION=3']
        if self.options.with_ruby:
            self.cpp_info.defines.append("WITH_RUBY=1")
        if self.options.without_ipv6:
            self.cpp_info.defines.append("AGENT_WITHOUT_IPV6=1")
        if self.options.shared:
            self.cpp_info.defines.append("SHARED_AGENT_LIB=1")
            self.cpp_info.defines.append("BOOST_ALL_DYN_LINK")
        
        if is_msvc(self):
            winver=str(self.options.winver)
            self.cpp_info.defines.append("WINVER=" + winver)
            self.cpp_info.defines.append("_WIN32_WINNT=" + winver)
            self.cpp_info.defines.append("BOOST_USE_WINAPI_VERSION=" + winver)

    def package(self):
        cmake = CMake(self)
        cmake.install()

        if self.options.cpack and self.settings.build_type == 'Release':
            dest = None
            if self.options.cpack_destination:
                dest = str(self.options.cpack_destination)
            else:
                dest = self.package_folder
                
            cpack = f"cpack -G {self.options.cpack_generator} -B {dest} . "
            if (self.options.cpack_name):                
                cpack = cpack + f"-D CPACK_PACKAGE_FILE_NAME={self.options.cpack_name}"

            print(f"Calling cpack: {cpack}")
            self.run(cpack, cwd=self.build_folder)
            


    
