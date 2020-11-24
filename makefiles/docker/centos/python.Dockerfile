FROM ortools/make:centos_swig AS env
RUN dnf -y update \
&& dnf -y install python38-devel python38-pip python38-wheel \
&& dnf clean all \
&& rm -rf /var/cache/dnf
RUN python3 -m pip install absl-py

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
