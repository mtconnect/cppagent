from conans import ConanFile, CMake, tools
from conan.tools.env import Environment
import os
import io
import re
import itertools as it
import glob

class RubyRiceConan(ConanFile):
    name = "mruby"
    version = "3.0.0"
    license = "https://github.com/mruby/mruby/blob/master/LICENSE"
    author = "Yukihiro “Matz” Matsumoto"
    homepage = "https://mruby.org/"
    url = "https://mruby.org/about/"
    description = "mruby conan recipe"
    topics = "ruby", "binding", "conan", "mruby"
    settings = "os", "compiler", "build_type", "arch"
    requires = "readline/8.1.2"

    options = { "shared": [True, False] }

    default_options = {
        "shared": False
        }

    _autotools = None
    _mruby_source = "mruby_source"
    _major, _minor, _patch = version.split('.')
    _ruby_version_dir = "ruby-{}.{}.0".format(_major, _minor)

    def generate(self):
        self.build_config = os.path.join(self.build_folder, self._mruby_source, "build_config", "mtconnect.rb")
        with open(self.build_config, "w") as f:
            f.write('''
MRuby::Build.new do |conf|
  conf.toolchain

  # include the default GEMs
  conf.gembox 'default'

  # C compiler settings
  conf.compilers.each do |c|
    c.defines << 'MRB_USE_DEBUG_HOOK'
    c.defines << 'MRB_WORD_BOXING'
    c.defines << 'MRB_INT64'
  end
  conf.enable_cxx_abi
  conf.enable_test  
#  conf.gembox 'full-core'
''')
            if self.settings.build_type == 'Debug':
                f.write('''
  conf.enable_debug
  conf.compilers.each do |c|
    c.flags << '-O0'
  end
''')
            f.write("end\n")

    def source(self):
        git = tools.Git(self._mruby_source)
        git.clone("https://github.com/mruby/mruby.git", "stable")
        
    def build(self):
        self.run("rake MRUBY_CONFIG=%s" % self.build_config,
                 cwd=self._mruby_source)

    def package(self):
        self.copy("*", src=os.path.join(self._mruby_source, "build", "host"))
        self.copy("*", src=os.path.join(self._mruby_source, "include"), dst="include")
        self.copy("*.h", src=os.path.join(self._mruby_source, "mrbgems"), dst="include")
        
    def package_info(self):        
        self.cpp_info.includedirs = ["include"]
        self.cpp_info.defines = "MRB_USE_DEBUG_HOOK", "MRB_WORD_BOXING", \
          "MRB_INT64", "MRB_USE_CXX_EXCEPTION", "MRB_USE_CXX_ABI", "MRB_DEBUG", \
          "MRB_USE_RATIONAL", "MRB_USE_COMPLEX"
        self.cpp_info.libdirs = ["lib"]
        self.cpp_info.bindirs = ["bin"]
        self.cpp_info.libs = ["mruby"]
        if self.settings.os != 'Windows':
            self.cpp_info.libs.append("m")

