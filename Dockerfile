# MTConnect C++ Agent Docker image build instructions
# Adapted from https://mtcup.org/Installing_C++_Agent_on_Ubuntu
# Agent source code https://github.com/mtconnect/cppagent

#----------------------------------------------------------------------------
# build os
#----------------------------------------------------------------------------

# base image - ubuntu has linux/arm/v7, linux/amd64, etc
FROM ubuntu:21.04 AS compile

# update os and add dependencies
# note: removed ruby - install in second stage
# note: dockerfiles run as root by default, so don't need sudo
# this follows recommended docker practices -
# see https://docs.docker.com/develop/develop-images/dockerfile_best-practices/#run
RUN apt-get update && apt-get install -y \
  build-essential \
  cmake \
  curl \
  git \
  libcppunit-dev \
  libxml2 \
  libxml2-dev \
  screen \
  && rm -rf /var/lib/apt/lists/*

#----------------------------------------------------------------------------
# build makefile
#----------------------------------------------------------------------------

# get latest cppagent source
RUN mkdir -p ~/agent/build \
  && cd ~/agent \
  && git clone https://github.com/mtconnect/cppagent.git

# build makefile using cmake
RUN cd ~/agent/build \
  && cmake -D CMAKE_BUILD_TYPE=Release ../cppagent/

#----------------------------------------------------------------------------
# compile app
#----------------------------------------------------------------------------

# compile source (~20mins - 3hrs for qemu)
RUN cd ~/agent/build && make

# install agent executable
RUN cp ~/agent/build/agent/agent /usr/local/bin

# copy simulator data to /etc/mtconnect
RUN mkdir -p /etc/mtconnect \
  && cd ~/agent/cppagent \
  && cp -r schemas simulator styles /etc/mtconnect

#----------------------------------------------------------------------------
# install
#----------------------------------------------------------------------------

# multi-stage build - bring only essentials from previous layers

# start with a base image
FROM ubuntu:21.04

# install ruby for simulator
RUN apt-get update && apt-get install -y \
  ruby \
  && rm -rf /var/lib/apt/lists/*

# change to a new non-root user for better security.
# this also adds the user to a group with the same name.
# --create-home creates a home folder, ie /home/<username>
RUN useradd --create-home agent
USER agent

# copy agent app and simulator files from previous layers
COPY --chown=agent:agent --from=compile /usr/local/bin/agent /usr/local/bin
COPY --chown=agent:agent --from=compile /etc/mtconnect /etc/mtconnect

# expose port
EXPOSE 5000

WORKDIR /etc/mtconnect

# default command - can override with docker run or docker-compose command.
# this runs the adapter simulator and the agent using the sample config file.
# note: must use shell form here instead of exec form, since we're running 
# multiple statements using shell commands (& and &&).
# see https://stackoverflow.com/questions/46797348/docker-cmd-exec-form-for-multiple-command-execution
CMD /usr/bin/ruby /etc/mtconnect/simulator/run_scenario.rb -l \
  /etc/mtconnect/simulator/VMC-3Axis-Log.txt & \
  cd /etc/mtconnect/simulator && agent debug agent.cfg

#----------------------------------------------------------------------------
# notes
#----------------------------------------------------------------------------

# After setup, the dirs look like this -
#
# /etc/mtconnect
#  |-- schemas - xsd files
#  |-- simulator - agent.cfg, simulator.rb, vmc-3axis.xml, log.txt
#  |-- styles - Streams.xsl, Streams.css, favicon.ico
#
# /usr/local/bin
#  |-- agent - the agent application
#
# the default simulator/agent.cfg has -
#   Devices = ../simulator/VMC-3Axis.xml
#   Files { styles { Path = ../styles } }
# see https://github.com/mtconnect/cppagent/blob/master/simulator/agent.cfg
