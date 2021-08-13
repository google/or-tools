# Create a virtual environment with all tools installed
# ref: https://hub.docker.com/_/alpine
FROM alpine:edge AS env
LABEL maintainer="corentinl@google.com"
# Install system build dependencies
ENV PATH=/usr/local/bin:$PATH
RUN apk add --no-cache git build-base linux-headers cmake xfce4-dev-tools
ENTRYPOINT ["/bin/sh", "-c"]
CMD ["/bin/sh"]

# SWIG
RUN apk add --no-cache swig

# Python
RUN apk add --no-cache python3-dev py3-pip py3-wheel
RUN python3 -m pip install absl-py mypy-protobuf

# Java
ENV JAVA_HOME=/usr/lib/jvm/java-1.8-openjdk
RUN apk add --no-cache openjdk8 maven

# .NET install
# see: https://dotnet.microsoft.com/download/dotnet-core/3.1
RUN apk add --no-cache wget icu-libs libintl
RUN dotnet_sdk_version=3.1.404 \
&& wget -O dotnet.tar.gz https://dotnetcli.azureedge.net/dotnet/Sdk/$dotnet_sdk_version/dotnet-sdk-$dotnet_sdk_version-linux-musl-x64.tar.gz \
&& dotnet_sha512='c6e73e88c69fa2c81eb572a64206fa6e94cb376230a05f14028c35aab202975c857973f9b5fac849c60d22f37563d8d53689c2605571e3b922bda2489e12346d' \
&& echo "$dotnet_sha512  dotnet.tar.gz" | sha512sum -c - \
&& mkdir -p /usr/share/dotnet \
&& tar -C /usr/share/dotnet -oxzf dotnet.tar.gz \
&& ln -s /usr/share/dotnet/dotnet /usr/bin/ \
&& chmod a+x /usr/bin/dotnet \
&& rm dotnet.tar.gz
# Trigger first run experience by running arbitrary cmd
RUN dotnet --info

################
##  OR-TOOLS  ##
################
FROM env AS devel
# Copy the snk key
COPY or-tools.snk /root/or-tools.snk
ENV DOTNET_SNK=/root/or-tools.snk

ARG SRC_GIT_BRANCH
ENV SRC_GIT_BRANCH ${SRC_GIT_BRANCH:-master}
ARG SRC_GIT_SHA1
ENV SRC_GIT_SHA1 ${SRC_GIT_SHA1:-unknown}

# Download sources
# use SRC_GIT_SHA1 to modify the command
# i.e. avoid docker reusing the cache when new commit is pushed
WORKDIR /root
RUN git clone -b "${SRC_GIT_BRANCH}" --single-branch https://github.com/google/or-tools \
&& echo "sha1: $(cd or-tools && git rev-parse --verify HEAD)" \
&& echo "expected sha1: ${SRC_GIT_SHA1}"

# Build third parties
FROM devel AS third_party
WORKDIR /root/or-tools
RUN make detect && make third_party

# Build project
FROM third_party AS build
RUN make detect_cc && make cc
RUN make detect_java && make java
RUN make detect_dotnet && make dotnet
