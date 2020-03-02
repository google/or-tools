# Create a virtual environment with all tools installed
# ref: https://hub.docker.com/_/centos
FROM centos:latest AS env
LABEL maintainer="mizux.dev@gmail.com"
# Install system build dependencies
ENV PATH=$PATH:/usr/local/bin
RUN yum -y update \
&& yum -y install git wget openssl-devel \
&& yum -y groupinstall "Development Tools" \
&& yum clean all \
&& rm -rf /var/cache/yum
# Install CMake 3.16.4
RUN wget "https://cmake.org/files/v3.16/cmake-3.16.4-Linux-x86_64.sh" \
&& chmod a+x cmake-3.16.4-Linux-x86_64.sh \
&& ./cmake-3.16.4-Linux-x86_64.sh --prefix=/usr/local/ --skip-license \
&& rm cmake-3.16.4-Linux-x86_64.sh
CMD [ "/bin/sh" ]



# Add the library src to our build env
FROM env AS cpp
# Create lib directory
WORKDIR /home/lib
# Bundle lib source
COPY . .
# CMake version
RUN cmake -version
# CMake configure
RUN cmake -H. -Bbuild -DBUILD_DEPS=ON
# CMake build
RUN cmake --build build --target all
# CMake build
RUN cmake --build build --target install



# Add the library src to our build env
FROM env AS python
# Swig install
RUN yum -y update \
&& yum -y install pcre-devel \
&& yum clean all \
&& rm -rf /var/cache/yum \
&& wget "https://downloads.sourceforge.net/project/swig/swig/swig-4.0.1/swig-4.0.1.tar.gz" \
&& tar xvf swig-4.0.1.tar.gz \
&& rm swig-4.0.1.tar.gz \
&& cd swig-4.0.1 \
&& ./configure --prefix=/usr \
&& make -j 4 \
&& make install \
&& cd .. \
&& rm -rf swig-4.0.1
# Python install
RUN yum -y update \
&& yum -y install python36-devel \
&& yum clean all \
&& rm -rf /var/cache/yum
# Create lib directory
WORKDIR /home/lib
# Bundle lib source
COPY . .
# CMake configure
RUN cmake -H. -Bbuild -DBUILD_DEPS=ON -DBUILD_PYTHON=ON
# CMake build
RUN cmake --build build --target all
# CMake build
RUN cmake --build build --target install



# Add the library src to our build env
FROM env AS java
# Swig install
RUN yum -y update \
&& yum -y install pcre-devel \
&& yum clean all \
&& rm -rf /var/cache/yum \
&& wget "https://downloads.sourceforge.net/project/swig/swig/swig-4.0.1/swig-4.0.1.tar.gz" \
&& tar xvf swig-4.0.1.tar.gz \
&& rm swig-4.0.1.tar.gz \
&& cd swig-4.0.1 \
&& ./configure --prefix=/usr \
&& make -j 4 \
&& make install \
&& cd .. \
&& rm -rf swig-4.0.1
# Java install
RUN yum -y update \
&& yum -y install java-1.8.0-openjdk  java-1.8.0-openjdk-devel \
&& yum clean all \
&& rm -rf /var/cache/yum
# Create lib directory
WORKDIR /home/lib
# Bundle lib source
COPY . .
# CMake configure
RUN cmake -H. -Bbuild -DBUILD_DEPS=ON -DBUILD_JAVA=ON
# CMake build
RUN cmake --build build --target all
# CMake build
RUN cmake --build build --target install



# Add the library src to our build env
FROM env AS dotnet
# Swig install
RUN yum -y update \
&& yum -y install pcre-devel \
&& yum clean all \
&& rm -rf /var/cache/yum \
&& wget "https://downloads.sourceforge.net/project/swig/swig/swig-4.0.1/swig-4.0.1.tar.gz" \
&& tar xvf swig-4.0.1.tar.gz \
&& rm swig-4.0.1.tar.gz \
&& cd swig-4.0.1 \
&& ./configure --prefix=/usr \
&& make -j 4 \
&& make install \
&& cd .. \
&& rm -rf swig-4.0.1
# .Net install
# see https://docs.microsoft.com/en-us/dotnet/core/install/linux-package-manager-centos7
RUN rpm -Uvh "https://packages.microsoft.com/config/rhel/7/packages-microsoft-prod.rpm" \
&& yum -y update \
&& yum -y install dotnet-sdk-3.1 \
&& yum clean all \
&& rm -rf /var/cache/yum
# Create lib directory
WORKDIR /home/lib
# Bundle lib source
COPY . .
# CMake configure
RUN cmake -S. -Bbuild -DBUILD_DEPS=ON -DBUILD_DOTNET=ON
# CMake build
RUN cmake --build build --target all
# CMake build
RUN cmake --build build --target install



# Create an install image
FROM env AS install_cpp
# Copy lib from devel to prod
COPY --from=cpp /usr/local /usr/local/
# Copy  sample
WORKDIR /home/sample
COPY ci/sample .
