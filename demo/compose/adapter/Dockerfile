# base image - alpine 3.18
FROM alpine:3.18 AS os

ARG LOG_FILE=""
ARG UID=1000
ARG GID=1000


# install ruby for simulator
RUN  apk update \
  && apk add ruby \
  && addgroup \
       --gid $GID \
       adapter \
  && adduser \
       --home /home/adapter \
       --disabled-password \
       --uid $UID \
       --ingroup adapter \
       adapter

USER adapter
WORKDIR /home/adapter

COPY --chown=adapter:adapter "./simulator/run_scenario.rb" .
COPY --chown=adapter:adapter "./demo/agent/${LOG_FILE}" machine.log

EXPOSE 7878

CMD ["ruby", "run_scenario.rb", "-l", "-m", "30", "machine.log"]



