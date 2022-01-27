FROM ortools/make:ubuntu_swig AS env
RUN apt-get update -qq \
&& apt-get install -yq python3-dev python3-pip \
 python3-wheel python3-venv \
 python3-numpy python3-pandas \
&& apt-get clean \
&& rm -rf /var/lib/apt/lists/* /tmp/* /var/tmp/*
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
