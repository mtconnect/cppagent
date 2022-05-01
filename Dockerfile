# MTConnect C++ Agent Docker image build instructions

# ---------------------------------------------------------------------
# notes
# ---------------------------------------------------------------------
# if building this with a private repo (eg cppagent_dev), need to pass
# in a github access token in order to clone the repo. 
#
# to build locally and push to docker hub, run this with something like -
#
#   docker buildx build \
#     --platform linux/amd64,linux/arm64,linux/arm/v7 \
#     --tag ladder99/agent2 \
#     --secret id=access_token,src=ACCESS_TOKEN \
#     --push \
#     .
#
# ACCESS_TOKEN is a file containing a GitHub personal access token,
# so can clone the private mtconnect cppagent_dev repo.
# keep it out of the github repo with .gitignore.
# this sets up a file with the contents accessible at /run/secrete/access_token.
# see https://vsupalov.com/docker-buildkit-features/
#
# to build in github actions, pass this into the docker/build-push-action@v2 step -
#
#   secrets: "access_token=${{ secrets.ACCESS_TOKEN }}"
#
# then use access token as below with
#
#   RUN --mount=type=secret,id=access_token \
#     git clone https://$(cat /run/secrets/access_token)@github.com...
#
# then should be able to run with something like
#
#   docker run -it -p5001:5000 ladder99/agent2:latest
#
# and visit http://localhost:5001 to see output
#
# for examples see https://www.docker.com/blog/multi-arch-images/

# ---------------------------------------------------------------------
# os
# ---------------------------------------------------------------------

# base image - ubuntu has linux/arm/v7, linux/amd64, etc
FROM ubuntu:latest AS os

# tzinfo hangs without this
ARG DEBIAN_FRONTEND=noninteractive

# ---------------------------------------------------------------------
# build
# ---------------------------------------------------------------------

FROM os AS build

# update os and add dependencies
# this follows recommended Docker practices -
# see https://docs.docker.com/develop/develop-images/dockerfile_best-practices/#run
# note: Dockerfiles run as root by default, so don't need sudo
RUN apt-get clean \
  && apt-get update \
  && apt-get install -y \
  build-essential python3.9 python3-pip git cmake make ruby rake \
  && pip install conan

# get latest source code
# could use `git checkout foo` to get a specific version here
RUN --mount=type=secret,id=access_token \
  cd ~ \
  && git clone --recurse-submodules --progress --depth 1 \
  https://$(cat /run/secrets/access_token)@github.com/mtconnect/cppagent_dev.git agent

# set some variables
ENV PATH=$HOME/venv3.9/bin:$PATH
ENV CONAN_PROFILE=conan/profiles/docker
ENV WITH_RUBY=True

# make installer
RUN cd ~/agent \
  && conan export conan/mqtt_cpp \
  && conan export conan/mruby \
  && conan install . -if build --build=missing \
  -pr $CONAN_PROFILE \
  -o run_tests=False \
  -o with_ruby=$WITH_RUBY

# limit cpus so don't run out of memory on local machine
# symptom: get error - "c++: fatal error: Killed signal terminated program cc1plus"
# can turn off if building in cloud
# ENV CONAN_CPU_COUNT=1

# compile source (~20mins - 4hrs for qemu)
RUN cd ~/agent && conan build . -bf build

# ---------------------------------------------------------------------
# release
# ---------------------------------------------------------------------

FROM os AS release

LABEL author="mtconnect" description="Docker image for the latest Development MTConnect C++ Agent"

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
  cd /etc/mtconnect/simulator && agent debug agent.cfg

# ---------------------------------------------------------------------
# note
# ---------------------------------------------------------------------
# After setup, the dirs look like this -
#
# /usr/local/bin
#  |-- agent - the cppagent application
#
# /etc/mtconnect
#  |-- schemas - xsd files
#  |-- simulator - agent.cfg, simulator.rb, vmc-3axis.xml, log.txt
#  |-- styles - Streams.xsl, Streams.css, favicon.ico
#
# the default simulator/agent.cfg has -
#   Devices = ../simulator/VMC-3Axis.xml
#   Files { styles { Path = ../styles } }
# see https://github.com/mtconnect/cppagent/blob/master/simulator/agent.cfg
