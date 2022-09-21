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

# Install Python
RUN pacman -Syu --noconfirm python python-pip python-numpy
RUN python -m pip install absl-py mypy-protobuf

WORKDIR /root
ADD or-tools_amd64_alpine-edge_python_v*.tar.gz .

RUN cd or-tools_*_v* && make test
