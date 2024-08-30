# ref: https://hub.docker.com/_/almalinux
FROM almalinux:9

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

# Install Java 8 SDK
RUN dnf -y update \
&& dnf -y install java-1.8.0-openjdk  java-1.8.0-openjdk-devel maven \
&& dnf clean all \
&& rm -rf /var/cache/dnf

WORKDIR /root
ADD or-tools_amd64_almalinux-9_java_v*.tar.gz .

RUN cd or-tools_*_v* && make test
