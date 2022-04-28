# MTConnect C++ Agent Docker image build instructions

# base image - ubuntu has linux/arm/v7, linux/amd64, etc
FROM ubuntu:21.04 AS compile

# get latest source code
#. can use `git checkout foo` to get a specific version here
RUN cd ~ \
  && git clone https://github.com/mtconnect/cppagent_dev.git agent \
  && cd agent

# tzinfo hangs without this
ARG DEBIAN_FRONTEND=noninteractive
ENV TZ=USA/Chicago

# update os and add dependencies
# note: Dockerfiles run as root by default, so don't need sudo
# this follows recommended Docker practices -
# see https://docs.docker.com/develop/develop-images/dockerfile_best-practices/#run
RUN apt update && apt install python3-pip -y


ENV PATH=$HOME/venv3.9/bin:$PATH
ENV CONAN_PROFILE=conan/profiles/gcc
ENV WITH_RUBY=True
RUN pip install conan \
  && conan export conan/mqtt_cpp \
  && conan export conan/mruby \
  && conan install . -if build --build=missing -pr $CONAN_PROFILE -o run_tests=False -o with_ruby=$WITH_RUBY
# RUN pip install conan
# RUN conan export conan/mqtt_cpp
# RUN conan export conan/mruby
# RUN conan install . -if build --build=missing -pr $CONAN_PROFILE -o run_tests=False -o with_ruby=$WITH_RUBY

# compile source (~20mins - 3hrs for qemu)
RUN conan build . -bf build

# # install agent executable
# # RUN cp ~/agent/build/agent/agent /usr/local/bin
# RUN cp build/agent/agent /usr/local/bin

# # copy simulator data to /etc/mtconnect
# RUN mkdir -p /etc/mtconnect \
#   && cd ~/agent \
#   && cp -r schemas simulator styles /etc/mtconnect

# # expose port
# EXPOSE 5000

# WORKDIR /etc/mtconnect

# # default command - can override with docker run or docker-compose command.
# # this runs the adapter simulator and the agent using the sample config file.
# # note: must use shell form here instead of exec form, since we're running 
# # multiple statements using shell commands (& and &&).
# # see https://stackoverflow.com/questions/46797348/docker-cmd-exec-form-for-multiple-command-execution
# CMD /usr/bin/ruby /etc/mtconnect/simulator/run_scenario.rb -l \
#   /etc/mtconnect/simulator/VMC-3Axis-Log.txt & \
#   cd /etc/mtconnect/simulator && agent debug agent.cfg
