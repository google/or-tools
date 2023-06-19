FROM ortools/make:archlinux_swig AS env
RUN pacman -Syu --noconfirm python python-pip \
 python-wheel python-virtualenv \
 python-numpy python-pandas
RUN python -m pip install --break-system-package \
 absl-py mypy-protobuf

FROM env AS devel
WORKDIR /home/project
COPY . .

FROM devel AS build
RUN make python

FROM build AS test
RUN make test_python

FROM build AS package
RUN make package_python
