FROM ortools/make:ubuntu_swig AS env
# see: https://docs.microsoft.com/en-us/dotnet/core/install/linux-ubuntu
RUN apt-get update -qq \
&& DEBIAN_FRONTEND=noninteractive apt-get install -yq wget apt-transport-https \
&& wget -q https://packages.microsoft.com/config/ubuntu/21.04/packages-microsoft-prod.deb -O packages-microsoft-prod.deb \
&& dpkg -i packages-microsoft-prod.deb \
&& apt-get update -qq \
&& DEBIAN_FRONTEND=noninteractive apt-get install -yq dotnet-sdk-3.1 dotnet-sdk-5.0 \
&& apt-get clean \
&& rm -rf /var/lib/apt/lists/* /tmp/* /var/tmp/*
# Trigger first run experience by running arbitrary cmd
RUN dotnet --info

FROM env AS devel
WORKDIR /home/project
COPY . .

FROM devel AS build
RUN make third_party
RUN make dotnet

FROM build AS test
RUN make test_dotnet

FROM build AS package
RUN make package_dotnet
