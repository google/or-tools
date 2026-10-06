# ref: https://hub.docker.com/_/debian
FROM debian:13

RUN apt-get update -qq \
&& apt-get install -yq wget build-essential cmake zlib1g-dev \
&& apt-get clean \
&& rm -rf /var/lib/apt/lists/* /tmp/* /var/tmp/*
ENTRYPOINT ["/bin/bash", "-c"]
CMD ["/bin/bash"]

# Install Python
RUN apt-get update -qq \
&& apt-get install -yq python3 python3-pip \
&& apt-get clean \
&& rm -rf /var/lib/apt/lists/* /tmp/* /var/tmp/*

WORKDIR /root
ADD or-tools_amd64_debian-13_python_v*.tar.gz .

RUN cd or-tools_*_v* && make test
