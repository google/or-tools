FROM ortools/cmake:ubuntu_swig AS env
# see: https://docs.microsoft.com/en-us/dotnet/core/install/linux-package-manager-ubuntu-1910
RUN apt-get update -qq \
&& apt-get install -yq wget apt-transport-https \
&& wget -q https://packages.microsoft.com/config/ubuntu/19.10/packages-microsoft-prod.deb -O packages-microsoft-prod.deb \
&& dpkg -i packages-microsoft-prod.deb \
&& apt-get update -qq \
&& apt-get install -yq dotnet-sdk-3.1 \
&& apt-get clean \
&& rm -rf /var/lib/apt/lists/* /tmp/* /var/tmp/*
# Trigger first run experience by running arbitrary cmd
RUN dotnet --info

FROM env AS devel
WORKDIR /home/project
COPY . .

FROM devel AS build
RUN cmake -S. -Bbuild -DBUILD_DEPS=ON -DBUILD_DOTNET=ON
RUN cmake --build build --target all
RUN cmake --build build --target install

FROM build AS test
RUN cmake --build build --target test

FROM env AS install_env
WORKDIR /home/sample
COPY --from=build /home/project/build/dotnet/packages/*.nupkg ./

FROM install_env AS install_devel
COPY cmake/samples/dotnet .

FROM install_devel AS install_build
RUN dotnet build

FROM install_build AS install_test
RUN dotnet test
