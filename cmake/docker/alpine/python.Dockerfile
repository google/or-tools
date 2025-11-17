FROM ortools/cmake:alpine_swig AS env

ENV PATH=/root/.local/bin:$PATH
RUN apk add --no-cache python3-dev py3-pip py3-wheel \
 py3-numpy py3-pandas py3-matplotlib py3-scipy
RUN rm -f /usr/lib/python3.*/EXTERNALLY-MANAGED \
&& python3 -m pip install absl-py mypy mypy-protobuf

FROM env AS devel
WORKDIR /home/project
COPY . .

ARG CMAKE_BUILD_PARALLEL_LEVEL
ENV CMAKE_BUILD_PARALLEL_LEVEL=${CMAKE_BUILD_PARALLEL_LEVEL:-4}

FROM devel AS build
RUN cmake -S. -Bbuild -DBUILD_PYTHON=ON -DVENV_USE_SYSTEM_SITE_PACKAGES=ON \
 -DBUILD_CXX_SAMPLES=OFF -DBUILD_CXX_EXAMPLES=OFF
RUN cmake --build build --target all -v
RUN cmake --build build --target install

FROM build AS test
RUN CTEST_OUTPUT_ON_FAILURE=1 cmake --build build --target test

FROM env AS install_env
WORKDIR /home/sample
COPY --from=build /home/project/build/python/dist/*.whl .
RUN python3 -m pip install *.whl

FROM install_env AS install_devel
COPY cmake/samples/python .

FROM install_devel AS install_build
RUN python3 -m compileall .

FROM install_build AS install_test
RUN python3 sample.py
