FROM ortools/make:fedora_swig AS env
RUN dnf -y update \
&& dnf -y install python3-devel python3-pip \
 python3-wheel \
 python3-numpy python3-pandas \
&& dnf clean all
RUN python3 -m pip install absl-py mypy-protobuf

FROM env AS devel
WORKDIR /home/project
COPY . .

FROM devel AS build
RUN make third_party
RUN make python

FROM build AS test
RUN make test_python

FROM build AS package
RUN make package_python
