from conans import ConanFile, CMake, tools, AutoToolsBuildEnvironment
import os
import io
import re
import itertools as it

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
    requires = ""

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
        conf_args = ["--with-static-linked-ext", "--enable-install-static-library", '--without-rdoc']
        build = None
        
        self._autotools.configure(args=conf_args, configure_dir=self._ruby_source, build=build)
        self._autotools.make()

    def package(self):
        self._autotools.install()
        self.copy("*", src=os.path.join(self._rice_source, "include"), dst=os.path.join("include", self._ruby_version_dir))
        
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
        lib = "libruby.{}.{}-static.a".format(self._major, self._minor)
        self.cpp_info.libs = [os.path.join(self.package_folder, "lib", lib)]

        defines = [x[2:] for x in config['CPPFLAGS'].split() if x.startswith('-D') and x != '-DNDEBUG']
        self.cpp_info.defines = defines
        
