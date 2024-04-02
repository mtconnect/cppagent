# MTConnect Public C++ Agent Docker image build instructions

# ---------------------------------------------------------------------
# notes
# ---------------------------------------------------------------------
#
# to build a cross-platform image, push to docker hub, and run it -
# (see CMakeLists.txt for current version number) -
#
#   docker buildx build \
#     -f docker/ubuntu/Dockerfile \
#     --platform linux/amd64,linux/arm64 \
#     --tag mtconnect/agent:latest \
#     --push \
#     .
#
#   To run tests, use `--build-arg WITH_TESTS=true`
#
#   docker run -it --rm --init --name agent -p5000:5000 -v ./agent/agent.cfg:/mtconnect/config/agent.cfg \
#        -v ./agent/Devices.xml:/mtconnect/config/Devices.xml \
#        mtconnect/agent
#
# then visit http://localhost:5000 to see the demo output.

# ---------------------------------------------------------------------
# os
# ---------------------------------------------------------------------

# base image - ubuntu has amd64, arm64 etc.
# 22.04 is the current long term support release, maintained until 2025-04.
FROM ubuntu:22.04 AS os

# tzinfo hangs without this
ARG DEBIAN_FRONTEND='noninteractive'

# ---------------------------------------------------------------------
# build
# ---------------------------------------------------------------------

FROM os AS build

# limit cpus so don't run out of memory on local machine
# symptom: get error - "c++: fatal error: Killed signal terminated program cc1plus"
# can turn off if building in cloud
ARG CONAN_CPU_COUNT=2
ARG WITH_RUBY='True'

# set some variables
ENV PATH="$HOME/venv3.9/bin:$PATH"
ENV CONAN_PROFILE='/root/agent/cppagent/conan/profiles/docker'

# update os and add dependencies
# note: Dockerfiles run as root by default, so don't need sudo
RUN  apt-get update \
  && apt-get install -y \
       autoconf \
       automake \
       build-essential \
       cmake \
       git \
       python3 \
       python3-pip \
       rake \
       ruby \
  && rm -rf /var/lib/apt/lists/* \
  && pip install conan

# make an agent directory and cd into it
WORKDIR /root/agent

# bring in the repo contents, minus .dockerignore files
COPY . cppagent

ARG WITH_TESTS=false
ARG WITH_TESTS_ARG=argument

# Build and optionally test
RUN  if [ -z "$WITH_TESTS" ] || [ "$WITH_TESTS" = "false" ]; then \
       WITH_TESTS_ARG="--test-folder="; \
     else \
       WITH_TESTS_ARG=""; \
     fi \
  && conan profile detect \
  && conan create cppagent \
       --build=missing \
       -c "tools.build:jobs=$CONAN_CPU_COUNT" \
       -o agent_prefix=mtc \
       -o cpack=True \
       -o "with_ruby=$WITH_RUBY" \
       -o cpack_destination=/root/agent \
       -o cpack_name=dist \
       -o cpack_generator=TGZ \
       -pr "$CONAN_PROFILE" \
       "${WITH_TESTS_ARG}" \
  && tar xf dist.tar.gz \
  && rm dist.tar.gz

# ---------------------------------------------------------------------
# release
# ---------------------------------------------------------------------

FROM os AS release

RUN  apt-get update

# TODO: How about shortening the description to MTConnect Agent or at least MTConnect C++ Agent?
LABEL author='mtconnect' description='MTConnect C++ Agent'

# change to a new non-root user for better security.
# this also adds the user to a group with the same name.
# --create-home creates a home folder, ie /home/<username>
ARG UID=1000
ARG GID=1000

RUN  groupadd \
     --gid $GID \
     agent \
  && useradd \
     --create-home \
     --uid $UID \
     --gid $GID \
     agent \
  && mkdir -p /mtconnect/config \
              /mtconnect/data \
              /mtconnect/log \
  && chown -R agent:agent /mtconnect

# install agent executable
COPY --from=build /root/agent/dist/bin/* /usr/bin/
COPY --from=build /root/agent/dist/lib/* /usr/lib/

# Install data
COPY --chown=agent:agent --from=build \
    /root/agent/dist/share/mtconnect/schemas/ \
    /mtconnect/data/schemas/
COPY --chown=agent:agent --from=build \
    /root/agent/dist/share/mtconnect/styles/ \
    /mtconnect/data/styles/

USER agent
WORKDIR /home/agent

# expose port
EXPOSE 5000

# The /mtconnect/config volume must be mounted with a valid agent.cfg
# and Devices.xml
# 
# For example:
#   if there is a ./agent directory with the config files, do the following:
# 
#   docker run -v ./agent/agent.cfg:/mtconnect/config/agent.cfg \
#              -v ./agent/Devices.xml:/mtconnect/config/Devices.xml
VOLUME ["/mtconnect/config", "/mtconnect/log", "/mtconnect/data"]

# default command - can override with docker run or docker-compose command.
# this runs the adapter simulator and the agent using the sample config file.
# note: must use shell form here instead of exec form, since we're running
# multiple statements using shell commands (& and &&).
# see https://stackoverflow.com/questions/46797348/docker-cmd-exec-form-for-multiple-command-execution

CMD ["/usr/bin/mtcagent", "run", "/mtconnect/config/agent.cfg"]

# ---------------------------------------------------------------------
# note
# ---------------------------------------------------------------------

# after setup, the dirs look like this -
#
# /usr/bin
#  |-- agent - the cppagent application
#
# /mtconnect/config - Configuration files agent.cfg and Devices.xml
#
# /mtconnect/data
#  |-- schemas - xsd files
#  |-- styles - styles.xsl, styles.css, favicon.ico, etc
#
# /home/agent - the user's directory
#
# /mtconnect/log - logging directory
