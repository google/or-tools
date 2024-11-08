# Create a virtual environment with all tools installed
# ref: https://hub.docker.com/r/opensuse/tumbleweed
FROM opensuse/tumbleweed AS base

# Install system build dependencies
ENV PATH=/usr/local/bin:$PATH
RUN zypper refresh \
&& zypper install -y git gcc gcc-c++ cmake \
&& zypper clean -a
ENV CC=gcc CXX=g++
ENTRYPOINT ["/usr/bin/bash", "-c"]
CMD [ "/usr/bin/bash" ]

FROM base AS swig
RUN zypper refresh \
&& zypper install -y swig \
&& zypper clean -a
