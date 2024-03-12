# Create a virtual environment with all tools installed
# ref: https://hub.docker.com/r/opensuse/leap
FROM opensuse/leap

# Install system build dependencies
ENV PATH=/usr/local/bin:$PATH
RUN zypper refresh \
&& zypper install -y git make gcc11 gcc11-c++ \
 wget which lsb-release util-linux pkgconfig autoconf libtool zlib-devel gzip \
&& zypper clean -a
ENV CC=gcc-11 CXX=g++-11
ENTRYPOINT ["/usr/bin/bash", "-c"]
CMD ["/usr/bin/bash"]

# Install CMake 3.28.3
RUN ARCH=$(uname -m) \
&& wget -q "https://cmake.org/files/v3.28/cmake-3.28.3-linux-${ARCH}.sh" \
&& chmod a+x cmake-3.28.3-linux-${ARCH}.sh \
&& ./cmake-3.28.3-linux-${ARCH}.sh --prefix=/usr/local/ --skip-license \
&& rm cmake-3.28.3-linux-${ARCH}.sh

WORKDIR /root
ADD or-tools_amd64_opensuse-leap_cpp_v*.tar.gz .

RUN cd or-tools_*_v* && make test
