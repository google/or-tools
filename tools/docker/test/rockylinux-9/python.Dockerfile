# ref: https://hub.docker.com/rockylinux/rockylinux
FROM rockylinux/rockylinux:9

#############
##  SETUP  ##
#############
#ENV PATH=/usr/local/bin:$PATH
RUN dnf -y update \
&& dnf -y group install 'Development Tools' \
&& dnf clean all \
&& rm -rf /var/cache/dnf
#CMD ["/usr/bin/bash"]

# Install Python
RUN dnf -y update \
&& dnf -y install python3 python3-devel python3-pip python3-numpy \
&& dnf clean all \
&& rm -rf /var/cache/dnf
RUN python3 -m pip install \
 absl-py mypy mypy-protobuf pandas

WORKDIR /root
ADD or-tools_amd64_rockylinux-9_python_v*.tar.gz .

RUN cd or-tools_*_v* && make test
