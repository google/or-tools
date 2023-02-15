# Create a virtual environment with all tools installed
# ref: https://hub.docker.com/_/archlinux/
FROM archlinux:latest AS env

#############
##  SETUP  ##
#############
ENV PATH=/usr/local/bin:$PATH
RUN pacman -Syu --noconfirm git base-devel cmake
ENTRYPOINT ["/bin/bash", "-c"]
CMD [ "/bin/bash" ]

WORKDIR /root
ADD or-tools_amd64_archlinux_cpp_v*.tar.gz .

RUN cd or-tools_*_v* && make test
