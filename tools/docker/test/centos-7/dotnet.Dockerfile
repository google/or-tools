FROM centos/devtoolset-7-toolchain-centos7
LABEL maintainer="corentinl@google.com"

USER root

RUN yum -y update \
&& yum -y groupinstall 'Development Tools' \
&& yum -y install which zlib-devel \
&& yum clean all \
&& rm -rf /var/cache/yum

# Install dotnet
RUN rpm -Uvh "https://packages.microsoft.com/config/rhel/7/packages-microsoft-prod.rpm" \
&& yum -y update \
&& yum -y install dotnet-sdk-2.1 \
&& yum clean all \
&& rm -rf /var/cache/yum

#ENV TZ=America/Los_Angeles
#RUN ln -snf /usr/share/zoneinfo/$TZ /etc/localtime && echo $TZ > /etc/timezone

WORKDIR /root
ADD or-tools_centos-7_v*.tar.gz .
