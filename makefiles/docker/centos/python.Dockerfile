FROM ortools/make:centos_swig AS env
RUN dnf -y update \
&& dnf -y install python36-devel python3-wheel \
&& dnf clean all \
&& rm -rf /var/cache/dnf

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
