# ref: https://hub.docker.com/rockylinux/rockylinux
FROM rockylinux/rockylinux:10

#############
##  SETUP  ##
#############
RUN dnf -y update \
&& dnf -y group install 'Development Tools' \
&& dnf -y install zlib-devel \
&& dnf clean all \
&& rm -rf /var/cache/dnf
#CMD ["/usr/bin/bash"]

# Install Java 21 SDK
RUN dnf -y update \
&& dnf -y install java-21-openjdk java-21-openjdk-devel maven \
&& dnf clean all \
&& rm -rf /var/cache/dnf
#ENV JAVA_HOME=/usr/lib/jvm/java

WORKDIR /root
ADD or-tools_amd64_rockylinux-10_java_v*.tar.gz .

RUN cd or-tools_*_v* && make test
