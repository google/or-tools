FROM minizinc/mznc2025:latest AS env

ENV SRC_GIT_BRANCH=v99bugfix

ENV TZ=America/Los_Angeles

RUN ln -snf /usr/share/zoneinfo/$TZ /etc/localtime && echo $TZ > /etc/timezone

RUN apt-get update -yq \
&& apt-get -y install pkg-config git wget autoconf libtool zlib1g-dev gawk g++ \
 curl make lsb-release

# Bazelisk
RUN wget \
https://github.com/bazelbuild/bazelisk/releases/download/v1.20.0/bazelisk-linux-amd64 \
&& chmod +x bazelisk-linux-amd64 \
&& mv bazelisk-linux-amd64 /root/bazel

FROM env AS devel
WORKDIR /root
RUN git clone -b "$SRC_GIT_BRANCH" --single-branch https://github.com/google/or-tools

FROM devel AS build
WORKDIR /root/or-tools
RUN /root/bazel build -c opt //ortools/flatzinc:fz

RUN ln -s /root/or-tools/bazel-bin/ortools/flatzinc/fz /entry_data/fzn-exec

RUN cp /root/or-tools/ortools/flatzinc/mznlib/*mzn /entry_data/mzn-lib

# Patch the run scripts
RUN  sed -i -e "s/-G/--fzn-flags --params=use_ls_only:true -p 1 -G/g" /minizinc/mzn-exec-fd
RUN  sed -i -e "s/-G/--fzn-flags --params=use_ls_only:true,num_workers:3 -G/g" /minizinc/mzn-exec-free
RUN  sed -i -e "s/-G/--fzn-flags --params=use_ls_only:true -G/g" /minizinc/mzn-exec-par
