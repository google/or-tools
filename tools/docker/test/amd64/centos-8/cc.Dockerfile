# ref: https://quay.io/repository/centos/centos
FROM quay.io/centos/centos:stream8
LABEL maintainer="corentinl@google.com"

RUN dnf -y update \
&& dnf -y groupinstall 'Development Tools' \
&& dnf -y install zlib-devel \
&& dnf clean all \
&& rm -rf /var/cache/dnf

#ENV TZ=America/Los_Angeles
#RUN ln -snf /usr/share/zoneinfo/$TZ /etc/localtime && echo $TZ > /etc/timezone

WORKDIR /root
ADD or-tools_amd64_centos-8_v*.tar.gz .

RUN cd or-tools_*_v* && make test_cc
