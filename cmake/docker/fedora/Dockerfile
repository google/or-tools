# Create a virtual environment with all tools installed
# ref: https://hub.docker.com/_/fedora
FROM fedora:latest AS env
LABEL maintainer="mizux.dev@gmail.com"
# Install system build dependencies
ENV PATH=$PATH:/usr/local/bin
RUN dnf -y update \
&& dnf -y install git wget \
&& dnf -y groupinstall "Development Tools" \
&& dnf -y install gcc-c++ cmake \
&& dnf clean all
CMD [ "/bin/sh" ]



# Add the library src to our build env
FROM env AS cpp
# Create lib directory
WORKDIR /home/liBundle lib source
COPY . .
# CMake version
RUN cmake -version
# CMake configure
RUN cmake -S. -Bbuild -DBUILD_DEPS=ON
# CMake build
RUN cmake --build build --target all -v
# CMake build
RUN cmake --build build --target install



FROM env AS python
# Swig install
RUN dnf -y update \
&& dnf -y install swig \
&& dnf clean all
# Python install
RUN dnf -y update \
&& dnf -y install python3 python3-devel \
&& dnf clean all
# Create lib directory
WORKDIR /home/lib
# Bundle lib source
COPY . .
# CMake configure
RUN cmake -S. -Bbuild -DBUILD_DEPS=ON -DBUILD_PYTHON=ON
# CMake build
RUN cmake --build build --target all -v
# CMake build
RUN cmake --build build --target install



FROM env AS java
# Swig install
RUN dnf -y update \
&& dnf -y install swig \
&& dnf clean all
# Java install
ENV JAVA_HOME=/usr/lib/jvm/jre-1.8.0-openjdk
RUN dnf -y update \
&& dnf -y install java-1.8.0-openjdk-devel \
&& dnf clean all
# Create lib directory
WORKDIR /home/lib
# Bundle lib source
COPY . .
# CMake configure
RUN cmake -S. -Bbuild -DBUILD_DEPS=ON -DBUILD_JAVA=ON
# CMake build
RUN cmake --build build --target all -v
# CMake build
RUN cmake --build build --target install




FROM env AS dotnet
# Swig install
RUN dnf -y update \
&& dnf -y install swig \
&& dnf clean all
# .Net install
# see: https://docs.microsoft.com/en-us/dotnet/core/install/linux-package-manager-fedora31
RUN rpm --import https://packages.microsoft.com/keys/microsoft.asc \
&& wget -q -O /etc/yum.repos.d/microsoft-prod.repo https://packages.microsoft.com/config/fedora/31/prod.repo \
&& dnf -y update \
&& dnf -y install dotnet-sdk-3.1 \
&& dnf clean all
# Trigger first run experience by running arbitrary cmd
RUN dotnet --info
# Create lib directory
WORKDIR /home/lib
# Bundle lib source
COPY . .
# CMake configure
RUN cmake -S. -Bbuild -DBUILD_DEPS=ON -DBUILD_DOTNET=ON
# CMake build
RUN cmake --build build --target all -v
# CMake build
RUN cmake --build build --target install



# Create an install image
FROM env AS install
# Copy lib from devel to prod
COPY --from=cpp /usr/local /usr/local/
# Copy  sample
WORKDIR /home/sample
COPY ci/sample .
