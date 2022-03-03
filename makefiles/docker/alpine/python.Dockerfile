FROM ortools/make:alpine_swig AS env
ENV PATH=/root/.local/bin:$PATH
RUN apk add --no-cache python3-dev py3-pip py3-wheel \
 py3-numpy py3-pandas py3-matplotlib

FROM env AS devel
WORKDIR /home/project
COPY . .

FROM devel AS build
RUN make third_party BUILD_DOTNET=OFF BUILD_JAVA=OFF BUILD_PYTHON=ON \
CMAKE_ARGS="-DVENV_USE_SYSTEM_SITE_PACKAGES=ON"
RUN make python

FROM build AS test
RUN make test_python

FROM build AS package
RUN make package_python
