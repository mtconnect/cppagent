from conans import ConanFile, tools


class MqttcppConan(ConanFile):
    name = "mqtt_cpp"
    version = "9.0.0"
    license = "Boost Software License, Version 1.0"
    author = "Takatoshi Kondo redboltz@gmail.com"
    url = "https://github.com/redboltz/mqtt_cpp"
    description = "MQTT client/server for C++14 based on Boost.Asio"
    topics = ("mqtt")
    settings = "os", "compiler", "build_type", "arch"
    requires = "boost/1.75.0"

    def source(self):
        git = tools.Git("repo")
        git.clone("https://github.com/redboltz/mqtt_cpp.git", branch="v" + self.version, shallow=True)

    def build(self):
        pass

    def package(self):
        self.copy("*.hpp", dst="include", src="repo/include")
