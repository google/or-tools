# Create a virtual environment with all tools installed
# ref: https://hub.docker.com/_/archlinux/
FROM archlinux:latest AS env

# Install system build dependencies
ENV PATH=/usr/local/bin:$PATH
RUN pacman -Syu --noconfirm git base-devel cmake
RUN cmake -version
RUN pacman -Syu --noconfirm nodejs emscripten
ENV PATH=/usr/lib/emscripten:$PATH
RUN emcc -v

# Add the library src to our build env
FROM env AS devel
WORKDIR /home/project
COPY . .

FROM devel AS build
RUN emcmake cmake -S. -Bbuild \
 -DCMAKE_BUILD_TYPE=Release -DBUILD_DEPS=ON \
 -DBUILD_EXAMPLES=OFF \
 -DUSE_COINOR=OFF \
 -DUSE_GLPK=OFF \
 -DUSE_HIGHS=OFF \
 -DUSE_PDLP=OFF \
 -DUSE_SCIP=OFF
RUN cmake --build build --target all -v
#RUN cmake --build build --target install -v

FROM build AS test
#RUN CTEST_OUTPUT_ON_FAILURE=1 cmake --build build --target test
RUN cd build && ctest -V
