# ref: https://hub.docker.com/_/ubuntu
FROM ubuntu:20.04

ENV DEBIAN_FRONTEND=noninteractive
RUN apt-get update \
&& apt-get install -yq make \
&& apt-get clean \
&& rm -rf /var/lib/apt/lists/* /tmp/* /var/tmp/*
ENTRYPOINT ["/bin/bash", "-c"]
CMD ["/bin/bash"]

WORKDIR /root
ADD or-tools_amd64_ubuntu-20.04_python_v*.tar.gz .

RUN cd or-tools_*_v* && make test
