FROM ortools/make:opensuse_swig AS env
RUN zypper update -y \
&& zypper install -y java-1_8_0-openjdk java-1_8_0-openjdk-devel maven \
&& zypper clean -a

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
