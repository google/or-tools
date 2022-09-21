# ref: https://quay.io/repository/centos/centos
FROM quay.io/centos/centos:stream8

RUN dnf -y update \
&& dnf -y groupinstall 'Development Tools' \
&& dnf clean all \
&& rm -rf /var/cache/dnf

# Install Python
RUN dnf -y update \
&& dnf -y install python3 python3-devel python3-pip \
&& dnf clean all \
&& rm -rf /var/cache/dnf

WORKDIR /root
ADD or-tools_amd64_centos-8_python_v*.tar.gz .

RUN cd or-tools_*_v* && make test
