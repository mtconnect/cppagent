# MTConnect C++ Agent Docker image build instructions

# run this with
#   docker buildx build --secret id=access_token,src=ACCESS_TOKEN .
# see https://vsupalov.com/docker-buildkit-features/
# ACCESS_TOKEN is a file containing a GitHub personal access token,
# so can clone the private mtconnect cppagent_dev repo.
# keep it out of the github repo with .gitignore.
# this sets up a file with the contents accessible at /run/secrete/access_token.
# see below at 
#   git clone https://$(cat /run/secrets/access_token)@github.com...

# ---------------------------------------------------------------------
# OS
# ---------------------------------------------------------------------

# base image - ubuntu has linux/arm/v7, linux/amd64, etc
FROM ubuntu:latest AS os

# tzinfo hangs without this
ARG DEBIAN_FRONTEND=noninteractive

# ---------------------------------------------------------------------
# Build
# ---------------------------------------------------------------------

FROM os AS build

# update os and add dependencies
# this follows recommended Docker practices -
# see https://docs.docker.com/develop/develop-images/dockerfile_best-practices/#run
# note: Dockerfiles run as root by default, so don't need sudo
# git cmake python3-pip ruby rake \
#	build-essential python3.9 python3-pip git cmake make \
# libxml2 cppunit
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

# limit cpus so don't run out of memory
# symptom: get error - "c++: fatal error: Killed signal terminated program cc1plus"
ENV CONAN_CPU_COUNT=2

# compile source (~20mins - 3hrs for qemu)
RUN cd ~/agent && conan build . -bf build


# # ---------------------------------------------------------------------
# # Release
# # ---------------------------------------------------------------------

# FROM os AS release

# LABEL author="mtconnect" description="Docker image for the latest Development MTConnect C++ Agent"

# EXPOSE 5000

# WORKDIR /agent/

# # COPY agent.cfg /agent/
# # COPY ./mtconnect-devicefiles/Devices/ /agent/devices/
# # COPY ./mtconnect-devicefiles/Assets/ /agent/assets
# # COPY docker-entrypoint.sh /agent/
# # # COPY --from=ubuntu-core app_build/simulator/ /agent/simulator
# # COPY --from=ubuntu-core app_build/schemas/ /agent/schemas
# # COPY --from=ubuntu-core app_build/styles/ /agent/styles
# # COPY --from=ubuntu-core app_build/build/bin/agent /agent/agent
# # RUN chmod +x /agent/agent \
# #   && chmod +x /agent/docker-entrypoint.sh

# # # ENTRYPOINT ["/bin/sh", "-x", "/agent/docker-entrypoint.sh"]

# # ---

# # install agent executable
# # RUN cp ~/agent/build/agent/agent /usr/local/bin
# RUN cp build/agent/agent /usr/local/bin

# # copy simulator data to /etc/mtconnect
# RUN mkdir -p /etc/mtconnect \
#   && cd ~/agent \
#   && cp -r schemas simulator styles /etc/mtconnect

# # # expose port
# # EXPOSE 5000

# # WORKDIR /etc/mtconnect

# # # default command - can override with docker run or docker-compose command.
# # # this runs the adapter simulator and the agent using the sample config file.
# # # note: must use shell form here instead of exec form, since we're running 
# # # multiple statements using shell commands (& and &&).
# # # see https://stackoverflow.com/questions/46797348/docker-cmd-exec-form-for-multiple-command-execution
# # CMD /usr/bin/ruby /etc/mtconnect/simulator/run_scenario.rb -l \
# #   /etc/mtconnect/simulator/VMC-3Axis-Log.txt & \
# #   cd /etc/mtconnect/simulator && agent debug agent.cfg
