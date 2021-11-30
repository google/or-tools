# Create a virtual environment with all tools installed
# ref: https://hub.docker.com/_/alpine
FROM alpine:edge AS env
LABEL maintainer="corentinl@google.com"
# Install system build dependencies
ENV PATH=/usr/local/bin:$PATH
RUN apk add --no-cache git build-base linux-headers cmake xfce4-dev-tools

WORKDIR /root
ADD or-tools_alpine-edge_v*.tar.gz .

RUN cd or-tools_*_v* && make test_cc
