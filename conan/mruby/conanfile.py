from conan import ConanFile
from conan.tools.files import get, copy
from conan.tools.layout import basic_layout
from conan.errors import ConanInvalidConfiguration
from conan.errors import ConanException
from conan.tools.microsoft.visual import msvc_runtime_flag, is_msvc
from conan.tools.microsoft import VCVars

import os
import io
import re
import itertools as it
import glob

class MRubyConan(ConanFile):
    name = "mruby"
    version = "3.2.0"
    license = "https://github.com/mruby/mruby/blob/master/LICENSE"
    author = "Yukihiro “Matz” Matsumoto"
    homepage = "https://mruby.org/"
    url = "https://mruby.org/about/"
    description = "mruby conan recipe"
    topics = "ruby", "binding", "conan", "mruby"
    settings = "os", "compiler", "build_type", "arch"

    options = { "shared": [True, False], "trace": [True, False] }

    requires = ["oniguruma/6.9.10"]

    default_options = {
        "shared": False,
        "trace": False
        }

    _autotools = None
    _major, _minor, _patch = version.split('.')
    _ruby_version_dir = "ruby-{}.{}.0".format(_major, _minor)

    def source(self):
        get(self, "https://github.com/mruby/mruby/archive/refs/tags/3.2.0.zip", strip_root=True, destination=self.source_folder)
        get(self, "https://github.com/mattn/mruby-onig-regexp/archive/refs/heads/master.zip",
            strip_root=True, destination=os.path.join(self.source_folder, 'mrbgems', 'mruby-onig-regexp'))
        
        
    def layout(self):
        basic_layout(self, src_folder="source")

    def generate(self):
        if is_msvc(self):
            VCVars(self).generate()

        self.build_config = os.path.join(self.build_folder, self.source_folder, "build_config", "mtconnect.rb")

        with open(self.build_config, "w") as f:
            f.write('''
# Work around possible onigmo regex package already installed somewhere

module MRuby
  module Gem
    class Specification
      alias orig_initialize initialize
      def initialize(name, &block)        
        if name =~ /onig-regexp/
          puts "!!! Removing methods from #{name}"
          class << self
            def search_package(name, version_query=nil);
              puts "!!! Do not allow for search using pkg-config"
              false
            end
          end
        end
        orig_initialize(name, &block)
      end

      alias orig_setup setup
      def setup
        orig_setup
        if @name =~ /onig-regexp/
          class << @build.cc
            def search_header_path(name)
              if name == 'oniguruma.h'
                true
              else
                false
              end
            end
          end
        end
      end
    end
  end
end

''')
            if is_msvc(self):
                if self.settings.arch == 'x86':
                    f.write("ENV['PROCESSOR_ARCHITECTURE'] = 'AMD32'\n")
                else:
                    f.write("ENV['PROCESSOR_ARCHITECTURE'] = 'AMD64'\n")                    
            
            f.write("MRuby::Build.new do |conf|\n")
            
            if is_msvc(self):
                f.write("  conf.toolchain :visualcpp\n")
            else:
                f.write("  conf.toolchain\n")

            onig_info = self.dependencies["oniguruma"].cpp_info
            onig_lib = onig_info.components["onig"].libs[0]
            onig_libdir = onig_info.libdirs[0]
            onig_includedir = onig_info.includedirs[0]

            f.write('''
  # Set up flags for oniguruma
  conf.cc.include_paths << '{}'
  conf.linker.libraries << '{}'
  conf.linker.library_paths << '{}'
'''.format(onig_includedir, onig_lib, onig_libdir))

            f.write('''
  # Set up flags for cross compiler and conan packages
  ldflags = ENV['LDFLAGS']
  conf.linker.flags.unshift(ldflags.split).flatten! if ldflags
  cppflags = ENV['CPPFLAGS']
  conf.cc.flags.unshift(cppflags.split).flatten! if cppflags

  Dir.glob("#{root}/mrbgems/mruby-*/mrbgem.rake") do |x|
    g = File.basename File.dirname x
    unless g =~ /^mruby-(?:bin-(debugger|mirb)|test)$/
      puts "Adding gem: #{g}"
      conf.gem :core => g
    end
  end

  # Add require
  conf.gem :github => 'mattn/mruby-require'

  # C compiler settings
  conf.compilers.each do |c|
    c.defines << 'MRB_USE_DEBUG_HOOK'
    c.defines << 'MRB_WORD_BOXING'
    c.defines << 'MRB_INT64'
''')
            if self.settings.os == 'Windows':
                f.write("    c.defines << 'MRB_FIXED_ARENA'\n")
  
            f.write('''
  end
  
  conf.enable_cxx_exception
  conf.enable_test  
''')
            if self.settings.build_type == 'Debug':
                f.write("  conf.enable_debug\n")
            if is_msvc(self):
                if self.settings.build_type == 'Debug':
                    f.write("  conf.compilers.each { |c|  c.flags << '/Od' }\n")
                f.write("  conf.compilers.each { |c| c.flags << '/std:c++17' }\n")
                f.write("  conf.compilers.each { |c| c.flags << '/%s' }\n" % msvc_runtime_flag(self))
            else:
                if self.settings.build_type == 'Debug':
                    f.write("  conf.compilers.each { |c| c.flags << '-O0 -g' }\n")
                if self.settings.compiler == 'gcc':
                    f.write("  conf.compilers.each { |c| c.flags << '-fPIC' }\n")                    
            
            f.write("end\n")

    def build(self):
        trace = ''
        if self.options.trace:
            trace = '--trace'
        # Show all current environment variables
        self.output.info("=== Environment Variables ===")
        self.run("set" if self.settings.os == "Windows" else "env", shell=True)

        # Show PATH, Ruby version, and Rake version inline
        self.output.info(f"PATH: {os.environ.get('PATH')}")
        self.run("rake %s MRUBY_CONFIG=%s MRUBY_BUILD_DIR=%s" % (trace, self.build_config, self.build_folder),
                 cwd=self.source_folder)

    def package(self):
        copy(self, "*", os.path.join(self.build_folder, "host", "bin"), os.path.join(self.package_folder, "bin"))
        copy(self, "*", os.path.join(self.build_folder, "host", "lib"), os.path.join(self.package_folder, "lib"))
        copy(self, "*", os.path.join(self.build_folder, "host", "include"), os.path.join(self.package_folder, "include"))
        copy(self, "*", os.path.join(self.source_folder, "include"), os.path.join(self.package_folder, "include"))
        copy(self, "*.h", os.path.join(self.source_folder, "mrbgems"), os.path.join(self.package_folder, "include"))
        
    def package_info(self):        
        self.cpp_info.includedirs = ["include"]

        if self.settings.os != 'Windows':
            ruby = os.path.join(self.package_folder, "bin", "mruby-config")
        else:
            ruby = os.path.join(self.package_folder, "bin", "mruby-config.bat")

        buf = io.StringIO()
        self.run("{} --cflags".format(ruby), stdout=buf, shell=True)

        defines = [d[2:] for d in buf.getvalue().split(' ') if d.startswith('/D') or d.startswith('-D')]
        self.cpp_info.defines = defines

        self.cpp_info.bindirs = ["bin"]
        if self.settings.os == 'Windows':
            self.cpp_info.libs = ["libmruby"]
        else:
            self.cpp_info.libs = ["mruby"]
            self.cpp_info.system_libs = ["m"]

