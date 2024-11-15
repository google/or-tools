# ref: https://hub.docker.com/_/almalinux
FROM almalinux:9

#############
##  SETUP  ##
#############
RUN dnf -y update \
&& dnf -y group install 'Development Tools' \
&& dnf -y install zlib-devel \
&& dnf clean all \
&& rm -rf /var/cache/dnf
#CMD ["/usr/bin/bash"]

# Install .Net
RUN dnf -y update \
&& dnf -y install dotnet-sdk-6.0 \
&& dnf clean all \
&& rm -rf /var/cache/dnf
# Trigger first run experience by running arbitrary cmd
RUN dotnet --info

WORKDIR /root
ADD or-tools_amd64_almalinux-9_dotnet_v*.tar.gz .

RUN cd or-tools_*_v* && make test
