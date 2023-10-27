# Create a virtual environment with all tools installed
# ref: https://hub.docker.com/_/alpine
FROM alpine:edge
# Install system build dependencies
RUN apk add --no-cache git clang-extra-tools
RUN apk add --no-cache python3 py3-pip \
&& rm -f /usr/lib/python3.*/EXTERNALLY-MANAGED \
&& python3 -m pip install black
