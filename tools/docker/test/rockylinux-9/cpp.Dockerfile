# ref: https://hub.docker.com/rockylinux/rockylinux
FROM rockylinux/rockylinux:9

#############
##  SETUP  ##
#############
#ENV PATH=/usr/local/bin:$PATH
RUN dnf -y update \
&& dnf -y group install 'Development Tools' \
&& dnf -y install zlib-devel cmake \
&& dnf clean all \
&& rm -rf /var/cache/dnf
#CMD ["/usr/bin/bash"]

WORKDIR /root
ADD or-tools_amd64_rockylinux-9_cpp_v*.tar.gz .

RUN cd or-tools_*_v* && make test
