FROM ubuntu:21.04

RUN apt-get update -qq \
&& DEBIAN_FRONTEND=noninteractive apt-get install -yq \
 build-essential zlib1g-dev \
&& apt-get clean \
&& rm -rf /var/lib/apt/lists/* /tmp/* /var/tmp/*

# Dotnet install
# see
# https://docs.microsoft.com/en-us/dotnet/core/install/linux-package-manager-ubuntu-2004
RUN apt-get update -qq \
&& apt-get install -yq wget apt-transport-https \
&& wget -q https://packages.microsoft.com/config/ubuntu/21.04/packages-microsoft-prod.deb \
&& dpkg -i packages-microsoft-prod.deb \
&& apt-get update -qq \
&& DEBIAN_FRONTEND=noninteractive apt-get install -yq dotnet-sdk-3.1 dotnet-sdk-5.0 \
&& apt-get clean \
&& rm -rf /var/lib/apt/lists/* /tmp/* /var/tmp/*
# Trigger first run experience by running arbitrary cmd
RUN dotnet --info

#ENV TZ=America/Los_Angeles
#RUN ln -snf /usr/share/zoneinfo/$TZ /etc/localtime && echo $TZ > /etc/timezone

WORKDIR /root
ADD or-tools_ubuntu-21.04_v*.tar.gz .

RUN cd or-tools_*_v* && make test_dotnet
