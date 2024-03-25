# ref: https://hub.docker.com/_/centos
FROM centos:7

#############
##  SETUP  ##
#############
RUN yum -y update \
&& yum -y groupinstall 'Development Tools' \
&& yum -y install zlib-devel \
&& yum clean all \
&& rm -rf /var/cache/yum

# Bump to gcc-11
RUN yum -y update \
&& yum -y install centos-release-scl \
&& yum -y install devtoolset-11 \
&& yum clean all \
&& echo "source /opt/rh/devtoolset-11/enable" >> /etc/bashrc
SHELL ["/bin/bash", "--login", "-c"]
ENTRYPOINT ["/usr/bin/bash", "--login", "-c"]
CMD ["/usr/bin/bash", "--login"]
# RUN g++ --version

# Install CMake 3.28.3
RUN ARCH=$(uname -m) \
&& wget -q "https://cmake.org/files/v3.28/cmake-3.28.3-linux-${ARCH}.sh" \
&& chmod a+x cmake-3.28.3-linux-${ARCH}.sh \
&& ./cmake-3.28.3-linux-${ARCH}.sh --prefix=/usr/local/ --skip-license \
&& rm cmake-3.28.3-linux-${ARCH}.sh

WORKDIR /root
ADD or-tools_amd64_centos-7_cpp_v*.tar.gz .

RUN cd or-tools_*_v* && make test
