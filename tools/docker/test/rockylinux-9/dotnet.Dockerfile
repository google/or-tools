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

# Install .Net
# see: https://learn.microsoft.com/en-us/dotnet/core/install/linux-scripted-manual#scripted-install
RUN wget -q "https://dot.net/v1/dotnet-install.sh" \
&& chmod a+x dotnet-install.sh \
&& ./dotnet-install.sh -c 3.1 -i /usr/local/bin \
&& ./dotnet-install.sh -c 6.0 -i /usr/local/bin
# Trigger first run experience by running arbitrary cmd
RUN dotnet --info

WORKDIR /root
ADD or-tools_amd64_rockylinux-9_dotnet_v*.tar.gz .

RUN cd or-tools_*_v* && make test
