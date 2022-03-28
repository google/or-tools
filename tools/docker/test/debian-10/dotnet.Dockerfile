FROM debian:10
LABEL maintainer="corentinl@google.com"

RUN apt-get update \
&& apt-get install -y -q build-essential zlib1g-dev \
&& apt-get clean \
&& rm -rf /var/lib/apt/lists/* /tmp/* /var/tmp/*

# Dotnet Install
# see https://docs.microsoft.com/en-us/dotnet/core/install/linux-debian#debian-10-
RUN apt-get update -qq \
&& apt-get install -qq gpg apt-transport-https \
&& wget -q "https://packages.microsoft.com/config/debian/10/packages-microsoft-prod.deb" -O packages-microsoft-prod.deb \
&& dpkg -i packages-microsoft-prod.deb \
&& rm packages-microsoft-prod.deb \
&& apt-get update -qq \
&& apt-get install -qq dotnet-sdk-3.1 dotnet-sdk-5.0 \
&& apt-get clean \
&& rm -rf /var/lib/apt/lists/* /tmp/* /var/tmp/*
# Trigger first run experience by running arbitrary cmd
RUN dotnet --info

#ENV TZ=America/Los_Angeles
#RUN ln -snf /usr/share/zoneinfo/$TZ /etc/localtime && echo $TZ > /etc/timezone

WORKDIR /root
ADD or-tools_debian-10_v*.tar.gz .

RUN cd or-tools_*_v* && make test_dotnet
