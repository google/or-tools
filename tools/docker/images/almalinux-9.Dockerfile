# ref: https://hub.docker.com/_/almalinux
FROM almalinux:9 AS env

#############
##  SETUP  ##
#############
ENV PATH=/usr/local/bin:$PATH
RUN dnf -y update \
&& dnf -y install git wget openssl-devel cmake \
&& dnf -y group install "Development Tools" \
&& dnf clean all \
&& rm -rf /var/cache/dnf
ENTRYPOINT ["/usr/bin/bash", "-c"]
CMD ["/usr/bin/bash"]

# Install SWIG 4.3.0
RUN dnf -y update \
&& dnf -y install pcre2-devel \
&& dnf clean all \
&& rm -rf /var/cache/dnf \
&& wget -q "https://downloads.sourceforge.net/project/swig/swig/swig-4.3.0/swig-4.3.0.tar.gz" \
&& tar xvf swig-4.3.0.tar.gz \
&& rm swig-4.3.0.tar.gz \
&& cd swig-4.3.0 \
&& ./configure --prefix=/usr \
&& make -j 4 \
&& make install \
&& cd .. \
&& rm -rf swig-4.3.0

# Install .Net
# see: https://learn.microsoft.com/en-us/dotnet/core/install/linux-scripted-manual#scripted-install
RUN wget -q "https://dot.net/v1/dotnet-install.sh" \
&& chmod a+x dotnet-install.sh \
&& ./dotnet-install.sh -c 8.0 -i /usr/local/bin
# Trigger first run experience by running arbitrary cmd
RUN dotnet --info

# Install Java 8 SDK
RUN dnf -y update \
&& dnf -y install java-1.8.0-openjdk java-1.8.0-openjdk-devel maven \
&& dnf clean all \
&& rm -rf /var/cache/dnf

# Install Python
RUN dnf -y update \
&& dnf -y install python3-devel python3-pip python3-numpy \
&& dnf clean all \
&& rm -rf /var/cache/dnf
RUN python3 -m pip install \
 absl-py mypy mypy-protobuf pandas

################
##  OR-TOOLS  ##
################
FROM env AS devel
ENV DISTRIBUTION=almalinux-9

WORKDIR /root
# Copy the snk key
COPY or-tools.snk /root/or-tools.snk
ENV DOTNET_SNK=/root/or-tools.snk

ARG SRC_GIT_BRANCH
ENV SRC_GIT_BRANCH ${SRC_GIT_BRANCH:-main}
ARG SRC_GIT_SHA1
ENV SRC_GIT_SHA1 ${SRC_GIT_SHA1:-unknown}

ARG OR_TOOLS_PATCH
ENV OR_TOOLS_PATCH ${OR_TOOLS_PATCH:-9999}

# Download sources
# use SRC_GIT_SHA1 to modify the command
# i.e. avoid docker reusing the cache when new commit is pushed
RUN git clone -b "${SRC_GIT_BRANCH}" --single-branch --depth=1 https://github.com/google/or-tools \
&& [[ $(cd or-tools && git rev-parse --verify HEAD) == ${SRC_GIT_SHA1} ]]
WORKDIR /root/or-tools

# C++
## build
FROM devel AS cpp_build
RUN make detect_cpp \
&& make cpp JOBS=8
## archive
FROM cpp_build AS cpp_archive
RUN make archive_cpp

# .Net
## build
FROM cpp_build AS dotnet_build
RUN sed -i 's/\(<SignAssembly>\).*\(<\/SignAssembly>\)/\1false\2/' ortools/dotnet/Google.OrTools*.csproj.in
RUN make detect_dotnet \
&& make dotnet JOBS=8
## archive
FROM dotnet_build AS dotnet_archive
RUN make archive_dotnet

# Java
## build
FROM cpp_build AS java_build
RUN make detect_java \
&& make java JOBS=8
## archive
FROM java_build AS java_archive
RUN make archive_java

# Python
## build
FROM cpp_build AS python_build
RUN make detect_python \
&& make python JOBS=8
## archive
FROM python_build AS python_archive
RUN make archive_python
