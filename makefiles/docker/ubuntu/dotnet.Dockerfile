FROM ortools/make:ubuntu_swig AS env

# Install .Net
# see: https://docs.microsoft.com/en-us/dotnet/core/install/linux-ubuntu
RUN apt-get update -qq \
&& apt-get install -yq dotnet-sdk-6.0 \
&& apt-get clean \
&& rm -rf /var/lib/apt/lists/* /tmp/* /var/tmp/*
# Trigger first run experience by running arbitrary cmd
RUN dotnet --info

FROM env AS devel
WORKDIR /home/project
COPY . .

FROM devel AS build
RUN make dotnet

FROM build AS test
RUN make test_dotnet

FROM build AS package
RUN make package_dotnet
