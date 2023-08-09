# MTConnect Public C++ Agent Docker image build instructions

# TODO: Do we need notes like this to be in `Dockerfile`? I think the docs might be a better place for this. IMHO most notes in this file should be moved to the docs.
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
#   # Note: In this case, I would suggest to map port `5000` to `5000`. The user can always change the port according to their needs.
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
ARG DEBIAN_FRONTEND='noninteractive'

# ---------------------------------------------------------------------
# build
# ---------------------------------------------------------------------

FROM os AS build

# limit cpus so don't run out of memory on local machine
# symptom: get error - "c++: fatal error: Killed signal terminated program cc1plus"
# can turn off if building in cloud
ARG CONAN_CPU_COUNT=10

ARG WITH_RUBY='True'

# set some variables
ENV PATH="$HOME/venv3.9/bin:$PATH"
ENV CONAN_PROFILE='conan/profiles/docker'

# update os and add dependencies
# note: Dockerfiles run as root by default, so don't need sudo
RUN apt-get update \
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
  && pip install conan -v 'conan==2.0.9'

# make an agent directory and cd into it
WORKDIR /root/agent

# bring in the repo contents, minus .dockerignore files
COPY . .

# make installer
RUN conan profile detect \
  && conan install . \
    --build=missing \
    -c "tools.build:jobs=$CONAN_CPU_COUNT" \
    -c tools.build:skip_test=True \
    -o "with_ruby=$WITH_RUBY" \
    -pr "$CONAN_PROFILE" \
  && conan create . \
    --build=missing \
    -c "tools.build:jobs=$CONAN_CPU_COUNT" \
    -c tools.build:skip_test=True \
    -o agent_prefix=mtc \
    -o cpack=True \
    -o "with_ruby=$WITH_RUBY" \
    -o zip_destination=/root/agent \
    -pr "$CONAN_PROFILE" \
    -tf ''

# ---------------------------------------------------------------------
# release
# ---------------------------------------------------------------------

FROM os AS release

# TODO: How about shortening the description to MTConnect Agent or at least MTConnect C++ Agent?
LABEL author='mtconnect' description='Docker image for the latest Production MTConnect C++ Agent'

ARG BIN_DIR='/usr/bin'
# FIXME: `/etc` is not the best place for the app data.
ARG MTCONNECT_DIR='/etc/mtconnect'

ENV MTCONNECT_DIR="$MTCONNECT_DIR"
ENV DEMO_DIR="$MTCONNECT_DIR/demo/agent"

# install ruby for simulator
RUN apt-get clean && apt-get update && apt-get install -y ruby zip

# change to a new non-root user for better security.
# this also adds the user to a group with the same name.
# -m creates a home folder, ie /home/<username>
# TODO: We might also want to merge these two RUN commands (the previous and the following one) into one, although it would be a bit less readable.
RUN useradd -m agent
USER agent
WORKDIR /home/agent

# install agent executable
COPY --chown=agent:agent --from=build /root/agent/build/bin/agent /usr/local/bin/

# Extract the data
RUN unzip *.zip

# Copy data to `MTCONNECT_DIR`
USER root
RUN cp /home/agent/agent-*-Linux/bin/* "$BIN_DIR" \
  && mkdir -p "$MTCONNECT_DIR" \
  && chown -R agent:agent "$MTCONNECT_DIR"

USER agent

RUN cp -r /home/agent/agent-*-Linux/schemas \
  /home/agent/agent-*-Linux/simulator \
  /home/agent/agent-*-Linux/styles \
  /home/agent/agent-*-Linux/demo \
  "$MTCONNECT_DIR"

# expose port
EXPOSE 5000

# default command - can override with docker run or docker-compose command.
# this runs the adapter simulator and the agent using the sample config file.
# note: must use shell form here instead of exec form, since we're running
# multiple statements using shell commands (& and &&).
# see https://stackoverflow.com/questions/46797348/docker-cmd-exec-form-for-multiple-command-execution
# FIXME: Test if this works using `mtcagent run "$DEMO_DIR/agent.cfg"` instead of running `cd $DEMO_DIR && mtcagent run agent.cfg`.
CMD /usr/bin/ruby "$MTCONNECT_DIR/simulator/run_scenario.rb" -p 7879 -l "$DEMO_DIR/mazak.txt" \
  & /usr/bin/ruby "$MTCONNECT_DIR/simulator/run_scenario.rb" -p 7878 -l "$DEMO_DIR/okuma.txt" \
  & mtcagent run "$DEMO_DIR/agent.cfg"

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