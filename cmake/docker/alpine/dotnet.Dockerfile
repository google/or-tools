FROM ortools/cmake:alpine_swig AS env

# .NET install
RUN apk add --no-cache wget icu-libs libintl \
&& mkdir -p /usr/share/dotnet \
&& ln -s /usr/share/dotnet/dotnet /usr/bin/dotnet

# see: https://dotnet.microsoft.com/download/dotnet-core/3.1
RUN dotnet_sdk_version=3.1.413 \
&& wget -qO dotnet.tar.gz https://dotnetcli.azureedge.net/dotnet/Sdk/$dotnet_sdk_version/dotnet-sdk-$dotnet_sdk_version-linux-musl-x64.tar.gz \
&& dotnet_sha512='46ffb31754b295cdb7dc615d5f905aa5842e3ada0e3f975217dfecbaa94e7b0190e86136fe9693d354b6ef88faa83e1c48496ffb1d644bd7ff437aeb48b9229c' \
&& echo "$dotnet_sha512  dotnet.tar.gz" | sha512sum -c - \
&& tar -C /usr/share/dotnet -oxzf dotnet.tar.gz \
&& rm dotnet.tar.gz
# Trigger first run experience by running arbitrary cmd
RUN dotnet --info

# see: https://dotnet.microsoft.com/download/dotnet-core/5.0
RUN dotnet_sdk_version=5.0.401 \
&& wget -qO dotnet.tar.gz https://dotnetcli.azureedge.net/dotnet/Sdk/$dotnet_sdk_version/dotnet-sdk-$dotnet_sdk_version-linux-musl-x64.tar.gz \
&& dotnet_sha512='a2077f4d1c9da9c69453b771cd239bad27f62379402cc5e1c74a1f2a960fd55efc85cc15eafbac11f17ea975895ce107fab4bbfc49880a0a14791e8ac13ca2de' \
&& echo "$dotnet_sha512  dotnet.tar.gz" | sha512sum -c - \
&& tar -C /usr/share/dotnet -oxzf dotnet.tar.gz \
&& rm dotnet.tar.gz
# Trigger first run experience by running arbitrary cmd
RUN dotnet --info

# Add the library src to our build env
FROM env AS devel
WORKDIR /home/project
COPY . .

FROM devel AS build
RUN cmake -S. -Bbuild -DBUILD_DOTNET=ON -DBUILD_CXX_SAMPLES=OFF -DBUILD_CXX_EXAMPLES=OFF
RUN cmake --build build --target all -v
RUN cmake --build build --target install

FROM build AS test
RUN CTEST_OUTPUT_ON_FAILURE=1 cmake --build build --target test

FROM env AS install_env
WORKDIR /home/sample
COPY --from=build /home/project/build/dotnet/packages/*.nupkg ./

FROM install_env AS install_devel
COPY cmake/samples/dotnet .

FROM install_devel AS install_build
RUN dotnet build

FROM install_build AS install_test
RUN dotnet test
