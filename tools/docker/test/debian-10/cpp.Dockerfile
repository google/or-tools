# ref: https://hub.docker.com/_/debian
FROM debian:10

RUN apt-get update \
&& apt-get install -y -q wget build-essential zlib1g-dev \
&& apt-get clean \
&& rm -rf /var/lib/apt/lists/* /tmp/* /var/tmp/*
ENTRYPOINT ["/bin/bash", "-c"]
CMD ["/bin/bash"]

# Install CMake 3.28.3
RUN ARCH=$(uname -m) \
&& wget -q "https://cmake.org/files/v3.28/cmake-3.28.3-linux-${ARCH}.sh" \
&& chmod a+x cmake-3.28.3-linux-${ARCH}.sh \
&& ./cmake-3.28.3-linux-${ARCH}.sh --prefix=/usr/local/ --skip-license \
&& rm cmake-3.28.3-linux-${ARCH}.sh

WORKDIR /root
ADD or-tools_amd64_debian-10_cpp_v*.tar.gz .

RUN cd or-tools_*_v* && make test
