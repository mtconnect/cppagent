from conan import ConanFile
from conan.tools.files import copy, get
import os

class MqttcppConan(ConanFile):
    name = "mqtt_cpp"
    version = "13.1.0"
    license = "Boost Software License, Version 1.0"
    author = "Takatoshi Kondo redboltz@gmail.com"
    url = "https://github.com/redboltz/mqtt_cpp"
    description = "MQTT client/server for C++14 based on Boost.Asio"
    topics = ("mqtt")
    settings = "os", "compiler", "build_type", "arch"
    requires = ["boost/1.79.0"]

    def source(self):
        get(self, "https://github.com/redboltz/mqtt_cpp/archive/refs/tags/v%s.zip" % self.version, \
            strip_root=True, destination=self.source_folder)

    def build(self):
        pass

    def package(self):
        copy(self, "*.hpp", os.path.join(self.source_folder, "include"), os.path.join(self.package_folder, "include"))
