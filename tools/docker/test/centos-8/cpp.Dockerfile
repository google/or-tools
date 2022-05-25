# ref: https://quay.io/repository/centos/centos
FROM quay.io/centos/centos:stream8

RUN dnf -y update \
&& dnf -y groupinstall 'Development Tools' \
&& dnf -y install zlib-devel \
&& dnf clean all \
&& rm -rf /var/cache/dnf

# Install system build dependencies
ENV PATH=/usr/local/bin:$PATH
RUN dnf -y update \
&& dnf -y install gcc-toolset-11 \
&& dnf clean all \
&& rm -rf /var/cache/dnf \
&& echo "source /opt/rh/gcc-toolset-11/enable" >> /etc/bashrc
SHELL ["/usr/bin/bash", "--login", "-c"]
ENTRYPOINT ["/usr/bin/bash", "--login", "-c"]
CMD ["/usr/bin/bash", "--login"]
# RUN g++ --version

# Install CMake 3.21.1
RUN ARCH=$(uname -m) \
&& wget -q "https://cmake.org/files/v3.21/cmake-3.21.1-linux-${ARCH}.sh" \
&& chmod a+x cmake-3.21.1-linux-${ARCH}.sh \
&& ./cmake-3.21.1-linux-${ARCH}.sh --prefix=/usr/local/ --skip-license \
&& rm cmake-3.21.1-linux-${ARCH}.sh

WORKDIR /root
ADD or-tools_amd64_centos-8_cpp_v*.tar.gz .

RUN cd or-tools_*_v* && make test
