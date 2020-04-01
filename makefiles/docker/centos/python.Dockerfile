FROM ortools/make:centos_swig AS env
RUN yum -y update \
&& yum -y install python36-devel python3-wheel \
&& yum clean all \
&& rm -rf /var/cache/yum

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
