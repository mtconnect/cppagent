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
#     --tag mtconnect/agent \
#     --push \
#     .
#
#   To run tests, use `--build-arg WITH_TESTS=true`
#
#   # Note: In this case, I would suggest to map port `5000` to `5000`. The user can always change the port according to their needs.
#   docker run -it --rm --init --name agent -p5000:5000 \
#        -v ./agent/agent.cfg:/mtconnect/config/agent.cfg \
#        -v ./agent/Devices.xml:/mtconnect/config/Devices.xml \
#        mtconnect/agent
#
# then visit http://localhost:5000 to see the demo output.

# ---------------------------------------------------------------------
# os
# ---------------------------------------------------------------------

# base image - alpine 3.18
FROM alpine:3.19 AS os

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
ENV HOME='/root'
ENV CONAN_PROFILE="$HOME/agent/cppagent/conan/profiles/docker"

# update os and add dependencies
# note: Dockerfiles run as root by default, so don't need sudo
# Note: `PIP_ROOT_USER_ACTION='ignore'` is required to remove `WARNING: Running pip as the 'root' user can result in broken permissions and conflicting behaviour with the system package manager. It is recommended to use a virtual environment instead: https://pip.pypa.io/warnings/venv`. The proper way to fix this warning is to create a non-root user before running `pip install`.
RUN  apk --update add \
       autoconf \
       automake \
       cmake \
       g++ \
       gcompat \
       git \
       linux-headers \
       make \
       py3-pip \
       python3 \
       ruby \
       ruby-rake \
    && python3 -m venv /python/conan \
    && . /python/conan/bin/activate \
    && PIP_ROOT_USER_ACTION='ignore' pip install conan

# make an agent directory and cd into it
WORKDIR /root/agent

# bring in the repo contents, minus .dockerignore files
COPY . cppagent

ARG WITH_TESTS=false
ARG WITH_TESTS_ARG=argument

# Build and optionally test. Unpack the dist to reduce overhead in the following release step.
RUN  if [ -z "$WITH_TESTS" ] || [ "$WITH_TESTS" = "false" ]; then \
       WITH_TESTS_ARG="--test-folder="; \
     else \
       WITH_TESTS_ARG=""; \
     fi \
  && . /python/conan/bin/activate \
  && conan profile detect \
  && conan create cppagent \
       --build=missing \
       -c "tools.build:jobs=$CONAN_CPU_COUNT" \
       -o agent_prefix=mtc \
       -o cpack=True \
       -o "with_ruby=$WITH_RUBY" \
       -o "cpack_destination=$HOME/agent" \
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

LABEL author='mtconnect' description='MTConnect C++ Agent'

# change to a new non-root user for better security.
# this also adds the user to a group with the same name.
# --create-home creates a home folder, ie /home/<username>
ARG UID=1000
ARG GID=1000

# install dependencies
RUN  apk update \
  && apk add \
       libstdc++ \
       libgcc \
  && addgroup \
       --gid $GID \
       agent \
  && adduser \
       --home /home/agent \
       --disabled-password \
       --uid $UID \
       --ingroup agent \
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
