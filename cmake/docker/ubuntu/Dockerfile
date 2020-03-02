# Create a virtual environment with all tools installed
# ref: https://hub.docker.com/_/ubuntu
FROM ubuntu:rolling AS env
LABEL maintainer="mizux.dev@gmail.com"
# Install system build dependencies
ENV PATH=$PATH:/usr/local/bin
RUN apt-get update -qq \
&& apt-get install -yq git wget libssl-dev build-essential \
&& apt-get clean \
&& rm -rf /var/lib/apt/lists/* /tmp/* /var/tmp/*
# Install CMake 3.16.4
RUN wget "https://cmake.org/files/v3.16/cmake-3.16.4.tar.gz" \
&& tar xzf cmake-3.16.4.tar.gz \
&& rm cmake-3.16.4.tar.gz \
&& cd cmake-3.16.4 \
&& ./bootstrap --prefix=/usr \
&& make \
&& make install \
&& cd .. \
&& rm -rf cmake-3.16.4
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



FROM env AS python
# Swig
RUN apt-get update -qq \
&& apt-get install -yq swig \
&& apt-get clean \
&& rm -rf /var/lib/apt/lists/* /tmp/* /var/tmp/*
# Python install
RUN apt-get update -qq \
&& apt-get install -yq python3-dev python3-pip \
&& apt-get clean \
&& rm -rf /var/lib/apt/lists/* /tmp/* /var/tmp/*
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



FROM env AS java
# Swig install
RUN apt-get update -qq \
&& apt-get install -yq swig \
&& apt-get clean \
&& rm -rf /var/lib/apt/lists/* /tmp/* /var/tmp/*
# Java install
RUN apt-get update -qq \
&& apt-get install -yq default-jdk \
&& apt-get clean \
&& rm -rf /var/lib/apt/lists/* /tmp/* /var/tmp/*
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




FROM env AS dotnet
# Swig install
RUN apt-get update -qq \
&& apt-get install -yq swig \
&& apt-get clean \
&& rm -rf /var/lib/apt/lists/* /tmp/* /var/tmp/*
# .Net install
# see: https://docs.microsoft.com/en-us/dotnet/core/install/linux-package-manager-ubuntu-1910
RUN apt-get update -qq \
&& apt-get install -yq wget apt-transport-https \
&& wget -q https://packages.microsoft.com/config/ubuntu/19.10/packages-microsoft-prod.deb -O packages-microsoft-prod.deb \
&& dpkg -i packages-microsoft-prod.deb \
&& apt-get update -qq \
&& apt-get install -yq dotnet-sdk-3.1 \
&& apt-get clean \
&& rm -rf /var/lib/apt/lists/* /tmp/* /var/tmp/*
# Trigger first run experience by running arbitrary cmd
RUN dotnet --info
# Create lib directory
WORKDIR /home/lib
# Bundle lib source
COPY . .
# CMake configure
RUN cmake -H. -Bbuild -DBUILD_DEPS=ON -DBUILD_DOTNET=ON
# CMake build
RUN cmake --build build --target all
# CMake build
RUN cmake --build build --target install



# Create an install image
FROM env AS install
# Copy lib from devel to prod
COPY --from=cpp /usr/local /usr/local/
# Copy  sample
WORKDIR /home/sample
COPY ci/sample .
