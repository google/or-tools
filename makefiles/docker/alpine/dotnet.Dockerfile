FROM ortools/make:alpine_swig AS env
RUN apk add --no-cache wget icu-libs	libintl
# .NET install
RUN dotnet_sdk_version=3.1.101 \
&& wget -O dotnet.tar.gz https://dotnetcli.azureedge.net/dotnet/Sdk/$dotnet_sdk_version/dotnet-sdk-$dotnet_sdk_version-linux-musl-x64.tar.gz \
&& dotnet_sha512='ce386da8bc07033957fd404909fc230e8ab9e29929675478b90f400a1838223379595a4459056c6c2251ab5c722f80858b9ca536db1a2f6d1670a97094d0fe55' \
&& echo "$dotnet_sha512  dotnet.tar.gz" | sha512sum -c - \
&& mkdir -p /usr/share/dotnet \
&& tar -C /usr/share/dotnet -oxzf dotnet.tar.gz \
&& ln -s /usr/share/dotnet/dotnet /usr/bin/dotnet \
&& rm dotnet.tar.gz
# Trigger first run experience by running arbitrary cmd
RUN dotnet --info

FROM env AS devel
WORKDIR /home/lib
COPY . .

FROM devel AS build
RUN make third_party
RUN make dotnet

FROM build AS test
RUN make test_dotnet

FROM build AS package
RUN make package_dotnet
