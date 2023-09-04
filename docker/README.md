# MTConnect Agent Docker Support

The MTConnect C++ Agent can be easily built and run on most platforms using docker. The ubuntu and alpine docker files perform the same function and provide 

## Build arguments and options

* ARG `CONAN_CPU_COUNT`: number of cpus to use in the build

    *default*: `2`

* ARG `WITH_RUBY`: include embedded ruby in the agent

    *default*: `True`

* ARG `WITH_TESTS`: `true` to build and run tests

    *default*: `false`

* Exposes port `5000` for the agent.

## Volumes

* `/mtconnect/config`: Configuration files `agent.cfg` and `Devices.xml`
* `/mtconnect/data`: Data files for `schemas`, `styles`, and `simulator`
* `/mtconnect/log`: Agent log files

## Ubuntu image

     cd docker/ubuntu

### For buildx multi-arch builds

     docker buildx build --platform="linux/arm64,linux/amd64" -t 'multi-arch/agent:latest' -f Dockerfile ../..

### For single architecture build

     docker build -t 'mtcagent:latest' ../.. 

## Alpine image

     cd docker/ubuntu

### For buildx multi-arch builds

     docker buildx build --platform="linux/arm64,linux/amd64" -t 'multi-arch/mtcagent:latest' -f Dockerfile ../..

### For single architecture build

     docker build -t 'mtcagent:latest' ../..

## Running the agent

### Run the agent with host port `5001` mapped to container port `5000`

    docker run --rm --name agent -it -p 5001:5000/tcp \
       -v ./mtconnect/conf:/mtconnect/config \
       -v ./mtconnect/log:/mtconnect/log \
       mtcagent

Runs the agent using the configuration files in the `./mtconnect/conf` directory and logs to the `./mtconnect/log`. The configuration is mapped so it will persist between runs and allows for custom configuration of the container.

# Replicator

A special version of the agent is supplied as an example. The replicator duplicates another agent using the latest version of the MTConnect agent capabilities. Once can use this to take a legacy agent and provide a 2.2 compliant agent with additional protocols.

## Build Replicator

     cd docker/replicator
     docker build -t replicator:latest' -f Dockerfile ../.. 

## Run the replicator

    docker run -it --rm -e SOURCE_URL=https://demo.mtconnect.org \
           -p 5001:5000 -v ./mtconnect/log:/mtconnect/log replicator:latest

Runs the replicator exposing port 5001 mapped to port 5000 in the container. The environment variable `SOURCE_URL` resolves to the agent being replicated.
