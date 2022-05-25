# Create a virtual environment with all tools installed
# ref: https://hub.docker.com/_/alpine
FROM alpine:edge AS env

#############
##  SETUP  ##
#############
ENV PATH=/usr/local/bin:$PATH
RUN apk add --no-cache git build-base linux-headers cmake
ENTRYPOINT ["/bin/sh", "-c"]
CMD ["/bin/sh"]

WORKDIR /root
ADD or-tools_amd64_alpine-edge_cpp_v*.tar.gz .

RUN cd or-tools_*_v* && make test
