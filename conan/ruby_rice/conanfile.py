from conans import ConanFile, CMake, tools, AutoToolsBuildEnvironment
import os
import io
import re
import itertools as it
import glob

class RubyRiceConan(ConanFile):
    name = "ruby_rice"
    version = "3.1.1"
    license = "https://github.com/ruby/ruby/blob/master/LEGAL"
    author = "Yukihiro “Matz” Matsumoto"
    homepage = "https://ruby-lang.org"
    url = "https://www.ruby-lang.org/en/about/"
    description = "Ruby for embedded integration using C++ Rice"
    topics = "ruby", "binding", "conan"
    settings = "os", "compiler", "build_type", "arch"
    requires = "readline/8.1.2", "libffi/3.4.2"

    options = { "shared": [True, False] }

    default_options = {
        "shared": True
        }

    _autotools = None
    _ruby_source = "ruby_source"
    _rice_source = "rice_source"
    _major, _minor, _patch = version.split('.')
    _ruby_version_dir = "ruby-{}.{}.0".format(_major, _minor)

    def source(self):
        tools.get("https://cache.ruby-lang.org/pub/ruby/{}.{}/ruby-{}.tar.gz".format(self._major, self._minor, self.version),
                  destination = self._ruby_source, strip_root = True)
        git = tools.Git(self._rice_source)
        git.clone("https://github.com/jasonroelofs/rice.git")
        
    def build(self):
        self._autotools = AutoToolsBuildEnvironment(self, win_bash=tools.os_info.is_windows)
        conf_args = ['--without-rdoc', "--enable-load-relative"]
        if not self.options.shared:
            conf_args.extend(["--with-static-linked-ext", "--enable-install-static-library"])
        else:
            conf_args.extend(['--enable-shared', '--enable-shared-libs'])
            
        if self.settings.build_type =='Debug':
            conf_args.extend(['optflags=-O3', '--enable-debug-env'])
        
        build = None
        
        self._autotools.configure(args=conf_args, configure_dir=self._ruby_source, build=build)
        self._autotools.make()

    def package(self):
        self._autotools.install()
        self.copy("*", src=os.path.join(self._rice_source, "include"), dst=os.path.join("include", self._ruby_version_dir))
        
        self.copy("*.a", src="enc", dst=os.path.join("lib-s", 'enc'))
        self.copy("encinit.o", src="enc", dst=os.path.join("lib-s", 'enc'))
        self.copy("*.a", src="ext", dst=os.path.join("lib-s", 'ext'))
        self.copy("extinit.o", src="ext", dst=os.path.join("lib-s", 'ext'))
        
    def package_info(self):
        ruby = os.path.join(self.package_folder, "bin", "ruby")
        buf = io.StringIO()
        self.run([ruby, "-r", "rbconfig",  "-e", 'puts "{" + RbConfig::CONFIG.map { |k, v| %{"#{k}": #{v.inspect} } }.join(",") + "}"'], output=buf)
        config = eval(buf.getvalue())

        only = re.compile("^-(O|std)")
        flags = [x for x in config['CFLAGS'].split() if not only.match(x) and not x.startswith('-W')]
        self.cpp_info.cxxflags = flags
        
        self.cpp_info.includedirs = [os.path.join("include", self._ruby_version_dir),
                                     os.path.join("include", self._ruby_version_dir, config['arch'])]
        
        self.cpp_info.libs = [os.path.join(self.package_folder, "lib", config['LIBRUBY'])]
        if not self.options.shared:
            [self.cpp_info.libs.append(l) for l in glob.glob(os.path.join(self.package_folder, "lib-s", "**", "*.a"), recursive=True)]
            self.cpp_info.libs.append(os.path.join(self.package_folder, "lib-s", "ext", "extinit.o"))
            self.cpp_info.libs.append(os.path.join(self.package_folder, "lib-s", "enc", "encinit.o"))

        defines = [x[2:] for x in config['CPPFLAGS'].split() if x.startswith('-D') and x != '-DNDEBUG']
        self.cpp_info.defines = defines

        if not self.options.shared:
            libflags = [x for x in config['LIBRUBYARG_STATIC'].split() if not x.startswith('-lruby')]
            
            try:
                ind = libflags.index('-framework')
                fw = libflags.pop(ind+1)
                libflags[ind] = "{} {}".format(libflags[ind], fw)
                
            except:
                pass
        #else:
        #    libflags = [config['LIBRUBYARG_SHARED']]
        
        self.user_info.RUBY_LIBRARIES = os.path.join(self.package_folder, "lib")
        self.cpp_info.exelinkflags = config['LDFLAGS'].split() # + libflags

