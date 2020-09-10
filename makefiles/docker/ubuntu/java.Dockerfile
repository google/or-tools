FROM ortools/make:ubuntu_swig AS env
RUN apt-get update -qq \
&& apt-get install -yq default-jdk maven \
&& apt-get clean \
&& rm -rf /var/lib/apt/lists/* /tmp/* /var/tmp/*
ENV JAVA_HOME=/usr/lib/jvm/default-java

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
