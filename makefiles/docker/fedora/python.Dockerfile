FROM ortools/make:fedora_swig AS env
RUN dnf -y update \
&& dnf -y install \
 python3 python3-devel python3-pip python3-six python3-wheel \
&& dnf clean all

FROM env AS devel
WORKDIR /home/lib
COPY . .

FROM devel AS build
RUN make third_party
RUN make python

FROM build AS test
RUN make test_python

FROM build AS package
RUN make package_python
