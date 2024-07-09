# ref: https://hub.docker.com/_/almalinux
FROM almalinux:9

#############
##  SETUP  ##
#############
RUN dnf -y update \
&& dnf -y groupinstall 'Development Tools' \
&& dnf -y install zlib-devel cmake \
&& dnf clean all \
&& rm -rf /var/cache/dnf
CMD ["/usr/bin/bash"]

WORKDIR /root
ADD or-tools_amd64_almalinux-9_cpp_v*.tar.gz .

RUN cd or-tools_*_v* && make test
