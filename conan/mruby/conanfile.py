from conans import ConanFile, CMake, tools
from conan.tools.env import Environment
import os
import io
import re
import itertools as it
import glob

class MRubyConan(ConanFile):
    name = "mruby"
    version = "3.1.0"
    license = "https://github.com/mruby/mruby/blob/master/LICENSE"
    author = "Yukihiro “Matz” Matsumoto"
    homepage = "https://mruby.org/"
    url = "https://mruby.org/about/"
    description = "mruby conan recipe"
    topics = "ruby", "binding", "conan", "mruby"
    settings = "os", "compiler", "build_type", "arch"

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
            if self.settings.os == 'Windows':
                if self.settings.arch == 'x86':
                    f.write("ENV['PROCESSOR_ARCHITECTURE'] = 'AMD32'\n")
                else:
                    f.write("ENV['PROCESSOR_ARCHITECTURE'] = 'AMD64'\n")                    
            
            f.write("MRuby::Build.new do |conf|\n")
            
            if self.settings.os == 'Windows':
                f.write("  conf.toolchain :visualcpp\n")
            else:
                f.write("  conf.toolchain\n")
                    
            f.write('''
  # include the default GEMs
  conf.gembox 'full-core'

  # Add regexp support
  conf.gem :github => 'mtconnect/mruby-onig-regexp', :branch => 'windows_porting'

  # C compiler settings
  conf.compilers.each do |c|
    c.defines << 'MRB_USE_DEBUG_HOOK'
    c.defines << 'MRB_WORD_BOXING'
    c.defines << 'MRB_INT64'
  end
  
  conf.enable_cxx_exception
  conf.enable_test  
''')
            if self.settings.build_type == 'Debug':
                f.write("  conf.enable_debug\n")
            if self.settings.os == 'Windows':
                if self.settings.build_type == 'Debug':
                    f.write("  conf.compilers.each { |c|  c.flags << '/Od' }\n")
                f.write("  conf.compilers.each { |c| c.flags << '/std:c++17' }\n")
                f.write("  conf.compilers.each { |c| c.flags << '/%s' }\n" % self.settings.compiler.runtime)
            else:
                if self.settings.build_type == 'Debug':
                    f.write("  conf.compilers.each { |c| c.flags << '-O0' }\n")
                if self.settings.compiler == 'gcc':
                    f.write("  conf.compilers.each { |c| c.flags << '-fPIC' }\n")                    
            
            f.write("end\n")

    def source(self):
        git = tools.Git(self._mruby_source)
        git.clone("https://github.com/mruby/mruby.git", "3.1.0")
        
    def build(self):
        self.run("rake MRUBY_CONFIG=%s" % self.build_config,
                 cwd=self._mruby_source)

    def package(self):
        self.copy("*", src=os.path.join(self._mruby_source, "build", "host", "bin"), dst="bin")
        self.copy("*", src=os.path.join(self._mruby_source, "build", "host", "lib"), dst="lib")
        self.copy("*", src=os.path.join(self._mruby_source, "build", "host", "include"), dst="include")
        self.copy("*", src=os.path.join(self._mruby_source, "include"), dst="include")
        self.copy("*.h", src=os.path.join(self._mruby_source, "mrbgems"), dst="include")
        
    def package_info(self):        
        self.cpp_info.includedirs = ["include"]

        if self.settings.os != 'Windows':
            ruby = os.path.join(self.package_folder, "bin", "mruby-config")
        else:
            ruby = os.path.join(self.package_folder, "bin", "mruby-config.bat")
        
        buf = io.StringIO()
        self.run([ruby, "--cflags"], output=buf)
        self.cpp_info.defines = [d[2:] for d in buf.getvalue().split(' ') if d.startswith('/D') or d.startswith('-D')]

        self.user_info.mruby = 'ON'

        self.cpp_info.bindirs = ["bin"]
        if self.settings.os == 'Windows':
            self.cpp_info.libs = ["libmruby"]
        else:
            self.cpp_info.libs = ["mruby", "m"]

