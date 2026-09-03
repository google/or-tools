# Create a virtual environment with all tools installed
# ref: https://hub.docker.com/_/alpine
FROM alpine:edge AS env

#############
##  SETUP  ##
#############
ENV PATH=/usr/local/bin:$PATH
RUN apk add --no-cache make
ENTRYPOINT ["/bin/sh", "-c"]
CMD ["/bin/sh"]

# Install Java
ENV JAVA_HOME=/usr/lib/jvm/java-21-openjdk
RUN apk add --no-cache openjdk-21-jdk maven

WORKDIR /root
ADD or-tools_amd64_alpine-edge_java_v*.tar.gz .

RUN cd or-tools_*_v* && make test
