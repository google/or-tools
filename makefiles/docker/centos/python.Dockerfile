FROM ortools/make:centos_swig AS env
RUN dnf -y update \
&& dnf -y install python311-devel python311-numpy python11-pip \
&& dnf clean all \
&& rm -rf /var/cache/dnf
RUN python3.11 -m pip install absl-py mypy-protobuf pandas

FROM env AS devel
WORKDIR /home/project
COPY . .

FROM devel AS build
RUN make python

FROM build AS test
RUN make test_python

FROM build AS package
RUN make package_python
