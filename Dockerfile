# MTConnect Public C++ Agent Docker image build instructions

# ---------------------------------------------------------------------
# notes
# ---------------------------------------------------------------------
#
# to build a cross-platform image, push to docker hub, and run it -
# (see CMakeLists.txt for current version number) -
#
#   docker buildx build \
#     --platform linux/amd64,linux/arm64 \
#     --tag mtconnect/agent:2.0.0.12_RC18 \
#     --push \
#     .
#
#   docker run -it --rm --init --name agent -p5001:5000 \
#     mtconnect/agent:2.0.0.12_RC18
#
# then visit http://localhost:5001 to see the demo output.

# ---------------------------------------------------------------------
# os
# ---------------------------------------------------------------------

# base image - ubuntu has amd64, arm64 etc.
# 22.04 is the current long term support release, maintained until 2025-04.
FROM ubuntu:22.04 AS os

# tzinfo hangs without this
ARG DEBIAN_FRONTEND=noninteractive

# ---------------------------------------------------------------------
# build
# ---------------------------------------------------------------------

FROM os AS build

# update os and add dependencies
# note: Dockerfiles run as root by default, so don't need sudo
RUN apt-get clean \
  && apt-get update \
  && apt-get install -y \
  build-essential python3.9 python3-pip git cmake make ruby rake \
  && pip install conan

# make an agent directory and cd into it
WORKDIR /root/agent

# bring in the repo contents, minus .dockerignore files
COPY . .

# set some variables
ENV PATH=$HOME/venv3.9/bin:$PATH
ENV CONAN_PROFILE=conan/profiles/docker
ENV WITH_RUBY=True
ENV WITH_TESTS=False

# limit cpus so don't run out of memory on local machine
# symptom: get error - "c++: fatal error: Killed signal terminated program cc1plus"
# can turn off if building in cloud
ENV CONAN_CPU_COUNT=1

# make installer
RUN conan export conan/mqtt_cpp \
  && conan export conan/mruby \
  && conan install . -if build --build=missing \
  -pr $CONAN_PROFILE \
  -o build_tests=$WITH_TESTS \
  -o run_tests=$WITH_TESTS \
  -o with_ruby=$WITH_RUBY

# compile source (~20mins - 4hrs for qemu)
RUN conan build . -bf build

# ---------------------------------------------------------------------
# release
# ---------------------------------------------------------------------

FROM os AS release

LABEL author="mtconnect" description="Docker image for the latest Production MTConnect C++ Agent"

# install ruby for simulator
RUN apt-get update && apt-get install -y ruby

# change to a new non-root user for better security.
# this also adds the user to a group with the same name.
# --create-home creates a home folder, ie /home/<username>
RUN useradd --create-home agent
USER agent

# install agent executable
COPY --chown=agent:agent --from=build /root/agent/build/bin/agent /usr/local/bin/

# copy data to /etc/mtconnect
COPY --chown=agent:agent --from=build /root/agent/schemas /etc/mtconnect/schemas
COPY --chown=agent:agent --from=build /root/agent/simulator /etc/mtconnect/simulator
COPY --chown=agent:agent --from=build /root/agent/styles /etc/mtconnect/styles

# expose port
EXPOSE 5000

WORKDIR /home/agent

# default command - can override with docker run or docker-compose command.
# this runs the adapter simulator and the agent using the sample config file.
# note: must use shell form here instead of exec form, since we're running 
# multiple statements using shell commands (& and &&).
# see https://stackoverflow.com/questions/46797348/docker-cmd-exec-form-for-multiple-command-execution
CMD /usr/bin/ruby /etc/mtconnect/simulator/run_scenario.rb -l \
  /etc/mtconnect/simulator/VMC-3Axis-Log.txt & \
  cd /etc/mtconnect/simulator && agent run agent.cfg


# ---------------------------------------------------------------------
# note
# ---------------------------------------------------------------------

# after setup, the dirs look like this -
#
# /usr/local/bin
#  |-- agent - the cppagent application
#
# /etc/mtconnect
#  |-- schemas - xsd files
#  |-- simulator - agent.cfg, simulator.rb, vmc-3axis.xml, log.txt
#  |-- styles - styles.xsl, styles.css, favicon.ico, etc
#
# /home/agent - the user's directory
