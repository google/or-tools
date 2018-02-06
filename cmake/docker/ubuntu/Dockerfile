FROM ubuntu:latest
LABEL maintainer="corentinl@google.com"

ADD setup.sh .
RUN ./setup.sh && apt-get clean && rm -rf /var/lib/apt/lists/* /tmp/* /var/tmp/*
