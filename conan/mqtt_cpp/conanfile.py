from conan import ConanFile
from conan.tools.files import copy, get
import os

class MqttcppConan(ConanFile):
    name = "mqtt_cpp"
    version = "13.2.1"
    license = "Boost Software License, Version 1.0"
    author = "Takatoshi Kondo redboltz@gmail.com"
    url = "https://github.com/redboltz/mqtt_cpp"
    description = "MQTT client/server for C++14 based on Boost.Asio"
    topics = ("mqtt")
    requires = ["boost/1.82.0"]
    no_copy_source = True
    exports_sources = "include/*"

    def source(self):
        get(self, "https://github.com/redboltz/mqtt_cpp/archive/refs/tags/v%s.zip" % self.version, \
            strip_root=True, destination=self.source_folder)

    def package_info(self):
        self.cpp_info.bindirs = []
        self.cpp_info.libdirs = []


    def package(self):
        copy(self, "*.hpp", self.source_folder, self.package_folder)

    def package_id(self):
        self.info.clear()
