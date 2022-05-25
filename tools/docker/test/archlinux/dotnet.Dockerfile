# Create a virtual environment with all tools installed
# ref: https://hub.docker.com/_/archlinux/
FROM archlinux:latest AS env

#############
##  SETUP  ##
#############
ENV PATH=/usr/local/bin:$PATH
RUN pacman -Syu --noconfirm make
ENTRYPOINT ["/bin/bash", "-c"]
CMD [ "/bin/bash" ]

# Install .Net
RUN pacman -Syu --noconfirm dotnet-sdk
# Trigger first run experience by running arbitrary cmd
RUN dotnet --info

WORKDIR /root
ADD or-tools_amd64_archlinux_dotnet_v*.tar.gz .

RUN cd or-tools_*_v* && make test
