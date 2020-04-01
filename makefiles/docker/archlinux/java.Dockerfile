FROM ortools/make:archlinux_swig AS env
RUN pacman -Syu --noconfirm jdk-openjdk maven

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
