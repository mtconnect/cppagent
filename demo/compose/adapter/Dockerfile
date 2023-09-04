# base image - alpine 3.18
FROM alpine:3.18 AS os

ARG LOG_FILE=""

# install ruby for simulator
RUN  apk update \
  && apk add ruby

ARG UID=1000
ARG GID=1000

RUN groupadd --gid $GID adapter
RUN useradd --create-home --uid $UID --gid $GID adapter

USER adapter
WORKDIR /home/adapter

COPY --chown=adapter:adapter "./simulator/run_scenario.rb" .
COPY --chown=adapter:adapter "./demo/agent/${LOG_FILE}" machine.log

EXPOSE 7878

CMD ["ruby", "run_scenario.rb", "-l", "machine.log"]



