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
#     --tag mtconnect/demo:latest \
#     --push \
#     -f demo/Dockerfile
#     .
#
#   To run tests, use `--build-arg WITH_TESTS=true`
#
#   docker run -it --rm --init --name agent -p5000:5000 mtconnect/demo
#
# then visit http://localhost:5000 to see the demo output.

# ---------------------------------------------------------------------
# Use the mtconnect/agent base image
# ---------------------------------------------------------------------
FROM mtconnect/agent as mtconnect

LABEL author='mtconnect' description='MTConnect C++ Agent Demo'

USER root

RUN apk --update add ruby

COPY --chown=agent:agent \
    ./simulator/*  \
    /mtconnect/data/simulator/
COPY --chown=agent:agent \
    ./demo/ \
    /mtconnect/data/demo/
COPY --chown=agent:agent \
    ./demo/agent/agent.dock \
    ./demo/agent/Devices.xml \
    /mtconnect/config/

USER agent
WORKDIR /home/agent

# expose port
EXPOSE 5000

# Allow the directories to be mounted
VOLUME ["/mtconnect/config", "/mtconnect/log", "/mtconnect/data"]

# default command - can override with docker run or docker-compose command.
# this runs the adapter simulator and the agent using the sample config file.
# note: must use shell form here instead of exec form, since we're running
# multiple statements using shell commands (& and &&).
# see https://stackoverflow.com/questions/46797348/docker-cmd-exec-form-for-multiple-command-execution

CMD /usr/bin/ruby /mtconnect/data/simulator/run_scenario.rb -p 7879 -l /mtconnect/data/demo/agent/mazak.txt \
  & /usr/bin/ruby /mtconnect/data/simulator/run_scenario.rb -p 7878 -l /mtconnect/data/demo/agent/okuma.txt \
  & mtcagent run /mtconnect/config/agent.dock

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
