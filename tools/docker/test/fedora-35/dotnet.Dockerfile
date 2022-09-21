# ref: https://hub.docker.com/_/fedora
FROM fedora:35

RUN dnf -y update \
&& dnf -y install git \
 wget which redhat-lsb-core pkgconfig autoconf libtool zlib-devel \
&& dnf -y groupinstall "Development Tools" \
&& dnf -y install gcc-c++ cmake \
&& dnf clean all

# Install .Net
# see: https://docs.microsoft.com/en-us/dotnet/core/install/linux-fedora
RUN dnf -y update \
&& dnf -y install dotnet-sdk-3.1 dotnet-sdk-6.0 \
&& dnf clean all
# Trigger first run experience by running arbitrary cmd
RUN dotnet --info

WORKDIR /root
ADD or-tools_amd64_fedora-35_dotnet_v*.tar.gz .

RUN cd or-tools_*_v* && make test
