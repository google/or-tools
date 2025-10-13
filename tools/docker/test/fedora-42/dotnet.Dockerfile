# ref: https://hub.docker.com/_/fedora
FROM fedora:42

RUN dnf -y update \
&& dnf -y install git \
 wget which lsb_release pkgconfig autoconf libtool zlib-devel \
&& dnf -y install @development-tools \
&& dnf -y install gcc-c++ cmake \
&& dnf clean all

# Install .Net
# see: https://docs.microsoft.com/en-us/dotnet/core/install/linux-fedora
RUN dnf -y update \
&& dnf -y install dotnet-sdk-8.0 \
&& dnf clean all
# Trigger first run experience by running arbitrary cmd
RUN dotnet --info

WORKDIR /root
ADD or-tools_amd64_fedora-42_dotnet_v*.tar.gz .

RUN cd or-tools_*_v* && make test
