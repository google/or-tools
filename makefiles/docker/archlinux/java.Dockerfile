FROM ortools/make:archlinux_swig AS env
RUN pacman -Syu --noconfirm jdk-openjdk maven
ENV JAVA_HOME=/usr/lib/jvm/default

FROM env AS devel
WORKDIR /home/project
COPY . .

FROM devel AS build
RUN make third_party BUILD_DOTNET=OFF BUILD_JAVA=ON BUILD_PYTHON=OFF
RUN make java

FROM build AS test
RUN make test_java

FROM build AS package
RUN make package_java
