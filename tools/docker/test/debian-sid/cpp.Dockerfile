# ref: https://hub.docker.com/_/debian
FROM debian:sid

RUN apt-get update \
&& apt-get install -y -q cmake build-essential zlib1g-dev \
&& apt-get clean \
&& rm -rf /var/lib/apt/lists/* /tmp/* /var/tmp/*
ENTRYPOINT ["/bin/bash", "-c"]
CMD ["/bin/bash"]

WORKDIR /root
ADD or-tools_amd64_debian-sid_cpp_v*.tar.gz .

RUN cd or-tools_*_v* && make test
