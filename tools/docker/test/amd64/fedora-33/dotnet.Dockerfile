FROM fedora:33
LABEL maintainer="corentinl@google.com"

RUN dnf -y update \
&& dnf -y install git \
 wget which redhat-lsb-core pkgconfig autoconf libtool zlib-devel \
&& dnf -y groupinstall "Development Tools" \
&& dnf -y install gcc-c++ cmake \
&& dnf clean all

# .Net Install
# see: https://docs.microsoft.com/en-us/dotnet/core/install/linux-fedora
RUN dnf -y update \
&& dnf -y install dotnet-sdk-3.1 dotnet-sdk-5.0 \
&& dnf clean all
# Trigger first run experience by running arbitrary cmd
RUN dotnet --info

WORKDIR /root
ADD or-tools_fedora-33_v*.tar.gz .

RUN cd or-tools_*_v* && make test_dotnet
