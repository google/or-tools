# Create a virtual environment with all tools installed
# ref: https://hub.docker.com/_/alpine
FROM alpine:edge
# Install system build dependencies
RUN apk add --no-cache git clang-dev
RUN apk add --no-cache python3 py3-pip \
&& python3 -m pip install yapf
