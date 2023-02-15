# ref: https://hub.docker.com/_/centos
FROM centos:7

#############
##  SETUP  ##
#############
RUN yum -y update \
&& yum -y groupinstall 'Development Tools' \
&& yum -y install zlib-devel \
&& yum clean all \
&& rm -rf /var/cache/yum

# Install .Net
# see https://docs.microsoft.com/en-us/dotnet/core/install/linux-centos#centos-7-
RUN rpm -Uvh https://packages.microsoft.com/config/centos/7/packages-microsoft-prod.rpm \
&& yum -y update \
&& yum -y install dotnet-sdk-3.1 dotnet-sdk-6.0 \
&& yum clean all \
&& rm -rf /var/cache/yum
# Trigger first run experience by running arbitrary cmd
RUN dotnet --info

WORKDIR /root
ADD or-tools_amd64_centos-7_dotnet_v*.tar.gz .

RUN cd or-tools_*_v* && make test
