FROM ortools/cmake:system_deps_swig AS env
ENV PATH=/root/.local/bin:$PATH
RUN pacman -Syu --noconfirm python \
 python-pip python-protobuf python-wheel python-virtualenv
RUN python -m pip install mypy_protobuf

FROM env AS devel
WORKDIR /home/project
COPY . .

FROM devel AS build
RUN cmake -S. -Bbuild -DBUILD_DEPS=OFF \
 -DUSE_COINOR=ON -DUSE_GLPK=ON -DUSE_SCIP=OFF \
 -DBUILD_PYTHON=ON -DBUILD_CXX_SAMPLES=OFF -DBUILD_CXX_EXAMPLES=OFF
RUN cmake --build build --target all -v
RUN cmake --build build --target install

FROM build AS test
RUN CTEST_OUTPUT_ON_FAILURE=1 cmake --build build --target test

FROM env AS install_env
WORKDIR /home/sample
COPY --from=build /home/project/build/python/dist/*.whl .
RUN python -m pip install *.whl

FROM install_env AS install_devel
COPY cmake/samples/python .

FROM install_devel AS install_build
RUN python -m compileall .

FROM install_build AS install_test
RUN python sample.py
