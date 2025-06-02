# ref: https://hub.docker.com/_/debian
FROM debian:13

RUN apt-get update \
&& apt-get install -y -q build-essential zlib1g-dev \
&& apt-get clean \
&& rm -rf /var/lib/apt/lists/* /tmp/* /var/tmp/*

# Install .Net
# see https://docs.microsoft.com/en-us/dotnet/core/install/linux-debian#debian-13-
RUN apt-get update -qq \
&& apt-get install -qq gpg apt-transport-https \
&& wget -q "https://packages.microsoft.com/config/debian/13/packages-microsoft-prod.deb" -O packages-microsoft-prod.deb \
&& dpkg -i packages-microsoft-prod.deb \
&& rm packages-microsoft-prod.deb \
&& apt-get update -qq \
&& apt-get install -qq dotnet-sdk-8.0 \
&& apt-get clean \
&& rm -rf /var/lib/apt/lists/* /tmp/* /var/tmp/*
# Trigger first run experience by running arbitrary cmd
RUN dotnet --info

#ENV TZ=America/Los_Angeles
#RUN ln -snf /usr/share/zoneinfo/$TZ /etc/localtime && echo $TZ > /etc/timezone

WORKDIR /root
ADD or-tools_amd64_debian-13_dotnet_v*.tar.gz .

RUN cd or-tools_*_v* && make test
