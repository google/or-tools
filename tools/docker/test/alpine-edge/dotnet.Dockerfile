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

# Install .Net
RUN apk add --no-cache wget icu-libs libintl \
&& mkdir -p /usr/share/dotnet \
&& ln -s /usr/share/dotnet/dotnet /usr/bin/dotnet

## .Net 3.1
## see: https://dotnet.microsoft.com/download/dotnet-core/3.1
RUN dotnet_sdk_version=3.1.415 \
&& wget -qO dotnet.tar.gz \
"https://builds.dotnet.microsoft.com/dotnet/Sdk/${dotnet_sdk_version}/dotnet-sdk-${dotnet_sdk_version}-linux-musl-x64.tar.gz" \
&& dotnet_sha512='20297eb436db2fe0cb3d8edfe4ad5b7c7871116616843314830533471a344f0ca943fbc5f92685113afc331a64c90f271245a36be1c232c364add936dd06d13d' \
&& echo "$dotnet_sha512  dotnet.tar.gz" | sha512sum -c - \
&& tar -C /usr/share/dotnet -oxzf dotnet.tar.gz \
&& rm dotnet.tar.gz

## .Net 8.0
## see: https://dotnet.microsoft.com/download/dotnet-core/8.0
RUN dotnet_sdk_version=8.0.408 \
&& wget -qO dotnet.tar.gz \
"https://dotnetcli.azureedge.net/dotnet/Sdk/$dotnet_sdk_version/dotnet-sdk-${dotnet_sdk_version}-linux-musl-x64.tar.gz" \
&& dotnet_sha512='0ab0c0d52985bde69b594454b5e1d9e1a6e003159656ee2972058d2960cfb0968dbe4d470d8eb21dcea41ff594976520e189a8e13afc44a419ca08e456df36e1' \
&& echo "$dotnet_sha512  dotnet.tar.gz" | sha512sum -c - \
&& tar -C /usr/share/dotnet -oxzf dotnet.tar.gz \
&& rm dotnet.tar.gz

# Trigger first run experience by running arbitrary cmd
RUN dotnet --info

WORKDIR /root
ADD or-tools_amd64_alpine-edge_dotnet_v*.tar.gz .

RUN cd or-tools_*_v* && make test
