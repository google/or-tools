# Create a virtual environment with all tools installed
# ref: https://quay.io/repository/centos/centos
FROM quay.io/centos/centos:stream AS base
# Install system build dependencies
ENV PATH=/usr/local/bin:$PATH
RUN dnf -y update \
&& dnf -y install \
 git wget which redhat-lsb-core openssl-devel pkgconfig autoconf libtool zlib-devel \
&& dnf -y groupinstall "Development Tools" \
&& dnf clean all \
&& rm -rf /var/cache/dnf

# Install system build dependencies
ENV PATH=/usr/local/bin:$PATH
RUN dnf -y update \
&& dnf -y install gcc-toolset-11 \
&& dnf clean all \
&& rm -rf /var/cache/dnf

RUN echo "source /opt/rh/gcc-toolset-11/enable" >> /etc/bashrc
SHELL ["/bin/bash", "--login", "-c"]

# Install CMake 3.28.3
RUN wget -q "https://cmake.org/files/v3.28/cmake-3.28.3-linux-x86_64.sh" \
&& chmod a+x cmake-3.28.3-linux-x86_64.sh \
&& ./cmake-3.28.3-linux-x86_64.sh --prefix=/usr/local/ --skip-license \
&& rm cmake-3.28.3-linux-x86_64.sh
CMD [ "/usr/bin/bash" ]

# Install SWIG 4.1.1
FROM base AS swig
RUN dnf -y update \
&& dnf -y install pcre2-devel \
&& dnf clean all \
&& rm -rf /var/cache/dnf \
&& wget -q "https://downloads.sourceforge.net/project/swig/swig/swig-4.1.1/swig-4.1.1.tar.gz" \
&& tar xvf swig-4.1.1.tar.gz \
&& rm swig-4.1.1.tar.gz \
&& cd swig-4.1.1 \
&& ./configure --prefix=/usr \
&& make -j 4 \
&& make install \
&& cd .. \
&& rm -rf swig-4.1.1
