FROM centos:7
LABEL maintainer="corentinl@google.com"

RUN yum -y update \
&& yum -y groupinstall 'Development Tools' \
&& yum -y install which zlib-devel java-1.8.0-openjdk java-1.8.0-openjdk-devel \
&& yum clean all \
&& rm -rf /var/cache/yum

#ENV TZ=America/Los_Angeles
#RUN ln -snf /usr/share/zoneinfo/$TZ /etc/localtime && echo $TZ > /etc/timezone

WORKDIR /root
ADD or-tools_centos-7_v*.tar.gz .
