FROM ortools/make:centos_swig AS env
RUN yum -y update \
&& yum -y install java-1.8.0-openjdk  java-1.8.0-openjdk-devel maven \
&& yum clean all \
&& rm -rf /var/cache/yum

FROM env AS devel
WORKDIR /home/lib
COPY . .

FROM devel AS build
RUN make third_party
RUN make java

FROM build AS test
RUN make test_java

FROM build AS package
RUN make package_java
