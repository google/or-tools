FROM ortools/make:ubuntu_swig AS env

# Install .Net
# see: https://docs.microsoft.com/en-us/dotnet/core/install/linux-ubuntu
# note: Ubuntu-22.04+ won't support dotnet-sdk-3.1
# see: https://github.com/dotnet/core/pull/7423/files
RUN apt-get update -qq \
&& apt-get install -yq wget apt-transport-https \
&& wget -q https://packages.microsoft.com/config/ubuntu/22.04/packages-microsoft-prod.deb \
&& dpkg -i packages-microsoft-prod.deb \
&& apt-get update -qq \
&& apt-get install -yq dotnet-sdk-6.0 \
&& apt-get clean \
&& rm -rf /var/lib/apt/lists/* /tmp/* /var/tmp/*
# Trigger first run experience by running arbitrary cmd
RUN dotnet --info

FROM env AS devel
WORKDIR /home/project
COPY . .

FROM devel AS build
RUN make dotnet CMAKE_ARGS="-DUSE_DOTNET_CORE_31=OFF"

FROM build AS test
RUN make test_dotnet

FROM build AS package
RUN make package_dotnet
