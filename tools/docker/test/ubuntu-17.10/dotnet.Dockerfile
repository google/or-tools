FROM ubuntu:17.10

RUN apt update \
&& apt install -y -q which build-essential zlib1g-dev \
&& apt clean \
&& rm -rf /var/lib/apt/lists/* /tmp/* /var/tmp/*

# Dotnet install
RUN apt-get update \
&& wget -q https://packages.microsoft.com/config/ubuntu/17.10/packages-microsoft-prod.deb \
&& dpkg -i packages-microsoft-prod.deb \
&& apt-get install -qq apt-transport-https \
&& apt-get update \
&& apt-get install -qq dotnet-sdk-2.1 \
&& apt-get clean \
&& rm -rf /var/lib/apt/lists/* /tmp/* /var/tmp/*

#ENV TZ=America/Los_Angeles
#RUN ln -snf /usr/share/zoneinfo/$TZ /etc/localtime && echo $TZ > /etc/timezone

WORKDIR /root
ADD or-tools_ubuntu-17.10_v*.tar.gz .
