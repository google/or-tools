# Create a virtual environment with all tools installed
# ref: https://hub.docker.com/_/alpine
FROM alpine:edge AS env

#############
##  SETUP  ##
#############
ENV PATH=/usr/local/bin:$PATH
RUN apk add --no-cache git build-base linux-headers make
ENTRYPOINT ["/bin/sh", "-c"]
CMD ["/bin/sh"]

# .NET install
RUN apk add --no-cache dotnet8-sdk
# Trigger first run experience by running arbitrary cmd
RUN dotnet --info

WORKDIR /root
ADD or-tools_amd64_alpine-edge_dotnet_v*.tar.gz .

RUN cd or-tools_*_v* && make test
