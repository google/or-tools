FROM centos:8
LABEL maintainer="corentinl@google.com"

RUN yum -y update \
&& yum -y groupinstall 'Development Tools' \
&& yum -y install which zlib-devel \
&& yum clean all \
&& rm -rf /var/cache/yum

# Install dotnet
# see https://docs.microsoft.com/en-us/dotnet/core/install/linux-package-manager-centos7
RUN rpm -Uvh "https://packages.microsoft.com/config/rhel/7/packages-microsoft-prod.rpm" \
&& yum -y update \
&& yum -y install dotnet-sdk-3.1 \
&& yum clean all \
&& rm -rf /var/cache/yum

#ENV TZ=America/Los_Angeles
#RUN ln -snf /usr/share/zoneinfo/$TZ /etc/localtime && echo $TZ > /etc/timezone

WORKDIR /root
ADD or-tools_centos-8_v*.tar.gz .
