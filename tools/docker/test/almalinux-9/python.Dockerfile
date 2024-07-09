# ref: https://hub.docker.com/_/almalinux
FROM almalinux:9

#############
##  SETUP  ##
#############
RUN yum -y update \
&& yum -y groupinstall 'Development Tools' \
&& yum clean all \
&& rm -rf /var/cache/yum

# Install Python
RUN yum -y update \
&& yum -y install python3 python3-devel python3-pip python3-numpy \
&& yum clean all \
&& rm -rf /var/cache/yum

WORKDIR /root
ADD or-tools_amd64_almalinux-9_python_v*.tar.gz .

RUN cd or-tools_*_v* && make test
