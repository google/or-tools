FROM ortools/make:opensuse_swig AS env
RUN zypper update -y \
&& zypper install -y wget tar libicu-devel
RUN dotnet_sdk_version=3.1.102 \
&& wget -O dotnet.tar.gz https://dotnetcli.azureedge.net/dotnet/Sdk/$dotnet_sdk_version/dotnet-sdk-$dotnet_sdk_version-linux-x64.tar.gz \
&& dotnet_sha512='9cacdc9700468a915e6fa51a3e5539b3519dd35b13e7f9d6c4dd0077e298baac0e50ad1880181df6781ef1dc64a232e9f78ad8e4494022987d12812c4ca15f29' \
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
