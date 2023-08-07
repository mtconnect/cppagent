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
RUN  apt-get clean \
  && apt-get update \
  && apt-get install -y \
       autoconf \
       automake \
       build-essential \
       cmake \
       git \
       python3 \
       python3-pip \
       rake \
       ruby

# Install conan version 2.0.9
RUN pip install conan -v "conan==2.0.9"

# make an agent directory and cd into it
WORKDIR /root/agent

# bring in the repo contents, minus .dockerignore files
COPY . .

ARG WITH_RUBY=True
ARG WITH_TESTS=False

# limit cpus so don't run out of memory on local machine
# symptom: get error - "c++: fatal error: Killed signal terminated program cc1plus"
# can turn off if building in cloud
ARG CONAN_CPU_COUNT=2

# set some variables
ENV CONAN_PROFILE=conan/profiles/docker

# make installer
RUN conan profile detect
RUN conan create . --build=missing -pr $CONAN_PROFILE -o with_ruby=$WITH_RUBY -o cpack=True \
      -o zip_destination=/root/agent -o agent_prefix=mtc \
      -c tools.build:skip_test=True -tf "" -c tools.build:jobs=$CONAN_CPU_COUNT

# ---------------------------------------------------------------------
# release
# ---------------------------------------------------------------------

FROM os AS release

LABEL author="mtconnect" description="Docker image for the latest Production MTConnect C++ Agent"
ARG BIN_DIR=/usr/bin
ARG MTCONNECT_DIR=/etc/mtconnect

ENV MTCONNECT_DIR=$MTCONNECT_DIR

# install ruby for simulator
RUN apt-get clean && apt-get update && apt-get install -y ruby zip

# change to a new non-root user for better security.
# this also adds the user to a group with the same name.
# --create-home creates a home folder, ie /home/<username>
RUN useradd --create-home agent
USER agent
WORKDIR /home/agent

# install agent executable
COPY --chown=agent:agent --from=build /root/agent/*.zip .

# copy data to /etc/mtconnect
RUN unzip *.zip

ENV ZIP_DIR=/home/agent/agent-*-Linux

USER root
RUN cp $ZIP_DIR/bin/* $BIN_DIR
RUN mkdir -p $MTCONNECT_DIR && chown -R agent:agent $MTCONNECT_DIR

USER agent
RUN cp -r $ZIP_DIR/schemas $ZIP_DIR/simulator $ZIP_DIR/styles $ZIP_DIR/demo $MTCONNECT_DIR/

# expose port
EXPOSE 5000

ENV DEMO_DIR=$MTCONNECT_DIR/demo/agent

# default command - can override with docker run or docker-compose command.
# this runs the adapter simulator and the agent using the sample config file.
# note: must use shell form here instead of exec form, since we're running
# multiple statements using shell commands (& and &&).
# see https://stackoverflow.com/questions/46797348/docker-cmd-exec-form-for-multiple-command-execution
CMD /usr/bin/ruby $MTCONNECT_DIR/simulator/run_scenario.rb -p 7879 -l $DEMO_DIR/mazak.txt & \
    /usr/bin/ruby $MTCONNECT_DIR/simulator/run_scenario.rb -p 7878 -l $DEMO_DIR/okuma.txt & \
    cd $DEMO_DIR && mtcagent run agent.cfg


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
