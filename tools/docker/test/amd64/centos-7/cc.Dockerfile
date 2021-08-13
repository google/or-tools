FROM centos:7
LABEL maintainer="corentinl@google.com"

RUN yum -y update \
&& yum -y groupinstall 'Development Tools' \
&& yum -y install zlib-devel \
&& yum clean all \
&& rm -rf /var/cache/yum

# Bump to gcc-9
RUN yum -y update \
&& yum -y install centos-release-scl \
&& yum -y install devtoolset-9 \
&& yum clean all \
&& echo "source /opt/rh/devtoolset-9/enable" >> /etc/bashrc
SHELL ["/bin/bash", "--login", "-c"]
ENTRYPOINT ["/usr/bin/bash", "--login", "-c"]
# RUN gcc --version

#ENV TZ=America/Los_Angeles
#RUN ln -snf /usr/share/zoneinfo/$TZ /etc/localtime && echo $TZ > /etc/timezone

WORKDIR /root
ADD or-tools_centos-8_v*.tar.gz .

RUN cd or-tools_*_v* && make test_cc
