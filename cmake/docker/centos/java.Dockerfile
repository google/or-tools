FROM ortools/cmake:centos_swig AS env
RUN yum -y update \
&& yum -y install java-1.8.0-openjdk  java-1.8.0-openjdk-devel maven \
&& yum clean all \
&& rm -rf /var/cache/yum

FROM env AS devel
WORKDIR /home/project
COPY . .

FROM devel AS build
RUN cmake -S. -Bbuild -DBUILD_DEPS=ON -DBUILD_JAVA=ON
RUN cmake --build build --target all
RUN cmake --build build --target install

FROM build AS test
RUN cmake --build build --target test

FROM env AS install_env
COPY --from=build /usr/local /usr/local/

FROM install_env AS install_devel
WORKDIR /home/sample
COPY cmake/samples/java .

FROM install_devel AS install_build
RUN mvn compile

FROM install_build AS install_test
RUN mvn test
