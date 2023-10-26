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
#     --tag mtconnect/replicator:latest \
#     --push \
#     .
#
#   For example, If the source agent is running on 192.168.5.100 port 5000 -- the following will replicate
#   the agent at that address
#
#   docker run -it --rm --init --name replicator -p5000:5000 -e SOURCE_URL=http://192.168.5.100:5000/ \
#     mtconnect/replicator
#
# then visit http://localhost:5000 to see the demo output.

# base image - mtconnect/agent
FROM mtconnect/agent as mtconnect

LABEL author='mtconnect' description='MTConnect C++ Agent Replicator'

USER agent

# Copy the agent configuration
COPY --chown=agent:agent \
    ./docker/replicator/Devices.xml \
    ./docker/replicator/agent.cfg \
    /mtconnect/config/

# expose port
EXPOSE 5000


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
