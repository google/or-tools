# Create a virtual environment with all tools installed
# ref: https://hub.docker.com/r/opensuse/leap
FROM opensuse/leap

# Install system build dependencies
ENV PATH=/usr/local/bin:$PATH
RUN zypper refresh \
&& zypper install -y git make gcc gcc-c++ cmake \
 wget which lsb-release util-linux pkgconfig autoconf libtool zlib-devel gzip \
&& zypper clean -a
ENV CC=gcc CXX=g++
ENTRYPOINT ["/usr/bin/bash", "-c"]
CMD ["/usr/bin/bash"]

WORKDIR /root
ADD or-tools_amd64_opensuse-leap_cpp_v*.tar.gz .

RUN cd or-tools_*_v* && make test
