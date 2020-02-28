# Create a virtual environment with all tools installed
# ref: https://hub.docker.com/_/alpine
FROM alpine:edge AS env
LABEL maintainer="mizux.dev@gmail.com"
# Install system build dependencies
ENV PATH=$PATH:/usr/local/bin
RUN apk add --no-cache git build-base linux-headers cmake
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
RUN apk add --no-cache swig
# Python install
RUN apk add --no-cache python3-dev py3-pip
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
RUN apk add --no-cache swig
# Java install
ENV JAVA_HOME=/usr/lib/jvm/java-1.8-openjdk
RUN apk add --no-cache openjdk8
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
RUN apk add --no-cache swig
# Add dependencies for disabling invariant mode (set in base image)
RUN apk add --no-cache wget icu-libs	libintl
# .NET install
RUN dotnet_sdk_version=3.1.101 \
&& wget -O dotnet.tar.gz https://dotnetcli.azureedge.net/dotnet/Sdk/$dotnet_sdk_version/dotnet-sdk-$dotnet_sdk_version-linux-musl-x64.tar.gz \
&& dotnet_sha512='ce386da8bc07033957fd404909fc230e8ab9e29929675478b90f400a1838223379595a4459056c6c2251ab5c722f80858b9ca536db1a2f6d1670a97094d0fe55' \
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
FROM env AS install_cpp
# Copy lib from devel to prod
COPY --from=cpp /usr/local /usr/local/
# Copy  sample
WORKDIR /home/sample
COPY ci/sample .
