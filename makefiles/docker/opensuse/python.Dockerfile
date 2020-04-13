FROM ortools/make:opensuse_swig AS env
RUN zypper update -y \
&& zypper install -y \
 python3 python3-pip python3-devel python3-wheel \
&& zypper clean -a

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
