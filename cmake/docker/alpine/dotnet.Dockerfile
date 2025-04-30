FROM ortools/cmake:alpine_swig AS env

# .NET install
RUN apk add --no-cache wget icu-libs libintl \
&& mkdir -p /usr/share/dotnet \
&& ln -s /usr/share/dotnet/dotnet /usr/bin/dotnet

## .Net 8.0
## see: https://dotnet.microsoft.com/download/dotnet-core/8.0
RUN dotnet_sdk_version=8.0.408 \
&& wget -qO dotnet.tar.gz \
"https://dotnetcli.azureedge.net/dotnet/Sdk/$dotnet_sdk_version/dotnet-sdk-${dotnet_sdk_version}-linux-musl-x64.tar.gz" \
&& dotnet_sha512='0ab0c0d52985bde69b594454b5e1d9e1a6e003159656ee2972058d2960cfb0968dbe4d470d8eb21dcea41ff594976520e189a8e13afc44a419ca08e456df36e1' \
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
RUN cmake -version
RUN cmake -S. -Bbuild -DBUILD_DOTNET=ON -DBUILD_CXX_SAMPLES=OFF -DBUILD_CXX_EXAMPLES=OFF
RUN cmake --build build --target all -v
RUN cmake --build build --target install -v

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
