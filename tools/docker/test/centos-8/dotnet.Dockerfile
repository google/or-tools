# ref: https://quay.io/repository/centos/centos
FROM quay.io/centos/centos:stream8

RUN dnf -y update \
&& dnf -y groupinstall 'Development Tools' \
&& dnf -y install zlib-devel \
&& dnf clean all \
&& rm -rf /var/cache/dnf

# Install .Net
# see https://docs.microsoft.com/en-us/dotnet/core/install/linux-package-manager-centos8
RUN dnf -y update \
&& dnf -y install dotnet-sdk-3.1 dotnet-sdk-6.0 \
&& dnf clean all \
&& rm -rf /var/cache/dnf
# Trigger first run experience by running arbitrary cmd
RUN dotnet --info

#ENV TZ=America/Los_Angeles
#RUN ln -snf /usr/share/zoneinfo/$TZ /etc/localtime && echo $TZ > /etc/timezone

WORKDIR /root
ADD or-tools_amd64_centos-8_dotnet_v*.tar.gz .

RUN cd or-tools_*_v* && make test
