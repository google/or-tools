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

# Install Java 8 SDK
RUN yum -y update \
&& yum -y install java-1.8.0-openjdk java-1.8.0-openjdk-devel maven \
&& yum clean all \
&& rm -rf /var/cache/yum
ENV JAVA_HOME=/usr/lib/jvm/java

WORKDIR /root
ADD or-tools_amd64_centos-7_java_v*.tar.gz .

RUN cd or-tools_*_v* && make test
