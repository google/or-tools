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

# see: https://dotnet.microsoft.com/download/dotnet-core/3.1
RUN dotnet_sdk_version=3.1.413 \
&& wget -qO dotnet.tar.gz https://builds.dotnet.microsoft.com/dotnet/Sdk/$dotnet_sdk_version/dotnet-sdk-$dotnet_sdk_version-linux-musl-x64.tar.gz \
&& dotnet_sha512='46ffb31754b295cdb7dc615d5f905aa5842e3ada0e3f975217dfecbaa94e7b0190e86136fe9693d354b6ef88faa83e1c48496ffb1d644bd7ff437aeb48b9229c' \
&& echo "$dotnet_sha512  dotnet.tar.gz" | sha512sum -c - \
&& tar -C /usr/share/dotnet -oxzf dotnet.tar.gz \
&& rm dotnet.tar.gz
# Trigger first run experience by running arbitrary cmd
RUN dotnet --info

# see: https://dotnet.microsoft.com/download/dotnet-core/6.0
RUN dotnet_sdk_version=6.0.100 \
&& wget -qO dotnet.tar.gz \
"https://builds.dotnet.microsoft.com/dotnet/Sdk/$dotnet_sdk_version/dotnet-sdk-${dotnet_sdk_version}-linux-musl-x64.tar.gz" \
&& dotnet_sha512='428082c31fd588b12fd34aeae965a58bf1c26b0282184ae5267a85cdadc503f667c7c00e8641892c97fbd5ef26a38a605b683b45a0fef2da302ec7f921cf64fe' \
&& echo "$dotnet_sha512  dotnet.tar.gz" | sha512sum -c - \
&& tar -C /usr/share/dotnet -oxzf dotnet.tar.gz \
&& rm dotnet.tar.gz
# Trigger first run experience by running arbitrary cmd
RUN dotnet --info

WORKDIR /root
ADD or-tools_amd64_alpine-edge_dotnet_v*.tar.gz .

RUN cd or-tools_*_v* && make test
