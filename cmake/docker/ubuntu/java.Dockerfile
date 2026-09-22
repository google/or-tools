ARG TARGETARCH
FROM ortools/cmake:${TARGETARCH:+${TARGETARCH}_}ubuntu_swig AS env

RUN apt-get update -qq \
&& DEBIAN_FRONTEND=noninteractive apt-get install -yq default-jdk maven \
&& apt-get clean \
&& rm -rf /var/lib/apt/lists/* /tmp/* /var/tmp/*
ENV JAVA_HOME=/usr/lib/jvm/default-java

FROM env AS devel
WORKDIR /home/project
COPY . .

ARG CMAKE_BUILD_PARALLEL_LEVEL
ENV CMAKE_BUILD_PARALLEL_LEVEL=${CMAKE_BUILD_PARALLEL_LEVEL:-4}

FROM devel AS build
RUN cmake -S. -Bbuild -DBUILD_JAVA=ON -DSKIP_GPG=ON \
-DBUILD_CXX_SAMPLES=OFF -DBUILD_CXX_EXAMPLES=OFF
RUN cmake --build build --target all -v
RUN cmake --build build --target install

FROM build AS test
RUN CTEST_OUTPUT_ON_FAILURE=1 cmake --build build --target test

FROM env AS install_env
WORKDIR /home/sample
COPY --from=build /home/project/build/java/ortools-linux-*/target/*.jar ./
RUN rm *-sources.jar \
&& for f in ortools-linux-*.jar; do mvn install:install-file -Dfile="$f"; break; done

COPY --from=build /home/project/build/java/ortools-java/target/*.jar ./
RUN rm *-sources.jar *-javadoc.jar \
&& for f in ortools-java-*.jar; do mvn install:install-file -Dfile="$f"; break; done

FROM install_env AS install_devel
COPY cmake/samples/java .

FROM install_devel AS install_build
RUN mvn compile

FROM install_build AS install_test
RUN mvn test
