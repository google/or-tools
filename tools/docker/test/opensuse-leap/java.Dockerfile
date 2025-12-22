# Create a virtual environment with all tools installed
# ref: https://hub.docker.com/r/opensuse/leap
FROM opensuse/leap

# Install system build dependencies
ENV PATH=/usr/local/bin:$PATH
RUN zypper refresh \
&& zypper install -y git gcc gcc-c++ \
 wget which lsb-release util-linux pkgconfig autoconf libtool zlib-devel \
&& zypper clean -a
ENV CC=gcc CXX=g++
ENTRYPOINT ["/usr/bin/bash", "-c"]
CMD ["/usr/bin/bash"]

# Java install (openjdk-8)
RUN zypper install -y java-21-openjdk java-21-openjdk-devel maven \
&& zypper clean -a

WORKDIR /root
ADD or-tools_amd64_opensuse-leap_java_v*.tar.gz .

RUN cd or-tools_*_v* && make test
