# Create a virtual environment with all tools installed
# ref: https://hub.docker.com/_/archlinux/
FROM archlinux/base AS env
LABEL maintainer="mizux.dev@gmail.com"
# Install system build dependencies
ENV PATH=$PATH:/usr/local/bin
RUN pacman -Syu --noconfirm git base-devel cmake
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



# Add the library src to our build env
FROM env AS python
# Swig install
RUN pacman -Syu --noconfirm swig
# Python install
RUN pacman -Syu --noconfirm python python-pip
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



# Add the library src to our build env
FROM env AS java
# Swig install
RUN pacman -Syu --noconfirm swig
# Java install
RUN pacman -Syu --noconfirm jdk-openjdk
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



# Add the library src to our build env
FROM env AS dotnet
# Swig install
RUN pacman -Syu --noconfirm swig
# .Net install
RUN pacman -Syu --noconfirm dotnet-sdk
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
