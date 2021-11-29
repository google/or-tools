FROM ortools/cmake:opensuse_swig AS env
# Install Java JDK and Maven
RUN zypper update -y \
&& zypper install -y java-17-openjdk-devel maven \
&& zypper clean -a
ENV PATH=/usr/share/maven/bin:$PATH

FROM env AS devel
WORKDIR /home/project
COPY . .

FROM devel AS build
RUN cmake -S. -Bbuild -DBUILD_JAVA=ON -DSKIP_GPG=ON \
 -DBUILD_CXX_SAMPLES=OFF -DBUILD_CXX_EXAMPLES=OFF
RUN cmake --build build --target all -v
RUN cmake --build build --target install -v

FROM build AS test
RUN CTEST_OUTPUT_ON_FAILURE=1 cmake --build build --target test -v

FROM env AS install_env
COPY --from=build /usr/local /usr/local/

FROM install_env AS install_devel
WORKDIR /home/sample
COPY cmake/samples/java .

FROM install_devel AS install_build
RUN mvn compile

FROM install_build AS install_test
RUN mvn test
