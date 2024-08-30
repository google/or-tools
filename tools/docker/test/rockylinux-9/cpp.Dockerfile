# ref: https://hub.docker.com/_/rockylinux
FROM rockylinux:9

#############
##  SETUP  ##
#############
ENV PATH=/usr/local/bin:$PATH
RUN dnf -y update \
&& dnf -y install git wget openssl-devel cmake \
&& dnf -y groupinstall "Development Tools" \
&& dnf clean all \
&& rm -rf /var/cache/dnf
CMD [ "/usr/bin/bash" ]

WORKDIR /root
ADD or-tools_amd64_rockylinux-9_cpp_v*.tar.gz .

RUN cd or-tools_*_v* && make test
