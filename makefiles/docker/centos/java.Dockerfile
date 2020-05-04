FROM ortools/make:centos_swig AS env
RUN dnf -y update \
&& dnf -y install java-1.8.0-openjdk  java-1.8.0-openjdk-devel maven \
&& dnf clean all \
&& rm -rf /var/cache/dnf

FROM env AS devel
WORKDIR /home/project
COPY . .

FROM devel AS build
RUN make third_party
RUN make java

FROM build AS test
RUN make test_java

FROM build AS package
RUN make package_java
