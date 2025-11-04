FROM ortools/cmake:system_deps_swig AS env

ENV PATH=/root/.local/bin:$PATH
#RUN pacman -Syu --noconfirm pybind11
RUN pacman -Syu --noconfirm python \
 python-setuptools python-wheel python-virtualenv \
 python-pip python-protobuf python-numpy python-pandas
RUN python -m pip install --break-system-package \
 absl-py mypy mypy-protobuf

FROM env AS devel
WORKDIR /home/project
COPY . .

FROM devel AS build
# Archlinux do not provide pybind11 protobuf package
RUN cmake -S. -Bbuild -DBUILD_DEPS=OFF \
 -DBUILD_pybind11=ON \
 -DBUILD_pybind11_abseil=ON \
 -DBUILD_pybind11_protobuf=ON \
 -DUSE_COINOR=ON \
 -DUSE_GLPK=ON \
 -DUSE_HIGHS=OFF \
 -DUSE_SCIP=ON \
 -DBUILD_PYTHON=ON \
 -DBUILD_CXX_SAMPLES=OFF -DBUILD_CXX_EXAMPLES=OFF \
 -DBUILD_googletest=ON
RUN cmake --build build --target all -v
RUN cmake --build build --target install

FROM build AS test
RUN CTEST_OUTPUT_ON_FAILURE=1 cmake --build build --target test

FROM env AS install_env
WORKDIR /home/sample
COPY --from=build /home/project/build/python/dist/*.whl .
RUN python -m pip install --break-system-packages *.whl

FROM install_env AS install_devel
COPY cmake/samples/python .

FROM install_devel AS install_build
RUN python -m compileall .

FROM install_build AS install_test
RUN python sample.py
