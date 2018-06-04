FROM ubuntu:18.04

RUN apt update \
&& apt install -y -q which build-essential zlib1g-dev \
&& apt clean \
&& rm -rf /var/lib/apt/lists/* /tmp/* /var/tmp/*

# Dotnet install
RUN apt-get update -qq \
&& apt-get install gpg apt-transport-https \
&& wget -qO- https://packages.microsoft.com/keys/microsoft.asc | gpg --dearmor > microsoft.asc.gpg \
&& mv microsoft.asc.gpg /etc/apt/trusted.gpg.d/ \
&& wget -q https://packages.microsoft.com/config/ubuntu/18.04/prod.list \
&& mv prod.list /etc/apt/sources.list.d/microsoft-prod.list \
&& apt-get update -qq \
&& apt-get install dotnet-sdk-2.1 \
&& apt clean \
&& rm -rf /var/lib/apt/lists/* /tmp/* /var/tmp/*

#ENV TZ=America/Los_Angeles
#RUN ln -snf /usr/share/zoneinfo/$TZ /etc/localtime && echo $TZ > /etc/timezone

WORKDIR /root
ADD or-tools_ubuntu-18.04_v*.tar.gz .
