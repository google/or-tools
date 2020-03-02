# Create a virtual environment with all tools installed
# ref: https://hub.docker.com/r/opensuse/tumbleweed
FROM opensuse/tumbleweed AS env
LABEL maintainer="mizux.dev@gmail.com"
# Install system build dependencies
ENV PATH=$PATH:/usr/local/bin
RUN zypper update -y \
&& zypper install -y git gcc gcc-c++ cmake \
&& zypper clean -a
ENV CC=gcc CXX=g++
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
RUN cmake -S. -Bbuild -DBUILD_DEPS=ON
# CMake build
RUN cmake --build build --target all -v
# CMake build
RUN cmake --build build --target install



FROM env AS python
# Swig install
RUN zypper update -y \
&& zypper install -y swig \
&& zypper clean -a
# Python install
RUN zypper update -y \
&& zypper install -y python3 python3-pip python3-devel \
&& zypper clean -a
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
RUN zypper update -y \
&& zypper install -y swig \
&& zypper clean -a
# Java install
RUN zypper update -y \
&& zypper install -y java-1_8_0-openjdk-devel \
&& zypper clean -a
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
RUN zypper update -y \
&& zypper install -y swig \
&& zypper clean -a
# .Net install
RUN zypper update -y \
&& zypper install -y wget tar libicu-devel
RUN dotnet_sdk_version=3.1.102 \
&& wget -O dotnet.tar.gz https://dotnetcli.azureedge.net/dotnet/Sdk/$dotnet_sdk_version/dotnet-sdk-$dotnet_sdk_version-linux-x64.tar.gz \
&& dotnet_sha512='9cacdc9700468a915e6fa51a3e5539b3519dd35b13e7f9d6c4dd0077e298baac0e50ad1880181df6781ef1dc64a232e9f78ad8e4494022987d12812c4ca15f29' \
&& echo "$dotnet_sha512  dotnet.tar.gz" | sha512sum -c - \
&& mkdir -p /usr/share/dotnet \
&& tar -C /usr/share/dotnet -oxzf dotnet.tar.gz \
&& ln -s /usr/share/dotnet/dotnet /usr/bin/dotnet \
&& rm dotnet.tar.gz
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
