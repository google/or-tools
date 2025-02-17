# ref: https://hub.docker.com/_/debian
FROM debian:13

RUN apt-get update \
&& apt-get install -yq wget build-essential cmake zlib1g-dev \
&& apt-get clean \
&& rm -rf /var/lib/apt/lists/* /tmp/* /var/tmp/*
ENTRYPOINT ["/bin/bash", "-c"]
CMD ["/bin/bash"]

WORKDIR /root
ADD or-tools_amd64_debian-13_python_v*.tar.gz .

RUN cd or-tools_*_v* && make test
