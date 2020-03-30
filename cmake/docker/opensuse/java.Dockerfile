FROM ortools:opensuse_swig AS env
RUN zypper update -y \
&& zypper install -y java-1_8_0-openjdk-devel maven \
&& zypper clean -a

FROM env AS devel
WORKDIR /home/lib
COPY . .

FROM devel AS build
RUN cmake -S. -Bbuild -DBUILD_DEPS=ON -DBUILD_JAVA=ON
RUN cmake --build build --target all -v
RUN cmake --build build --target install

FROM build AS test
RUN cmake --build build --target test

FROM env AS install_env
COPY --from=build /usr/local /usr/local/

FROM install_env AS install_devel
WORKDIR /home/sample
COPY ci/sample .

FROM install_devel AS install_build
RUN cmake -S. -Bbuild -DBUILD_DOTNET=ON
RUN cmake --build build --target all -v
RUN cmake --build build --target install

FROM install_build AS install_test
RUN cmake --build build --target test
