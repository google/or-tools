FROM minizinc/mznc2018:1.0

ENV SRC_GIT_BRANCH master

RUN apt-get update

RUN apt-get -y install pkg-config git wget autoconf libtool zlib1g-dev gawk g++ curl cmake make lsb-release python-dev gfortran

ENV TZ=America/Los_Angeles
RUN ln -snf /usr/share/zoneinfo/$TZ /etc/localtime && echo $TZ > /etc/timezone

WORKDIR /root

RUN git clone -b "$SRC_GIT_BRANCH" --single-branch https://github.com/google/or-tools

WORKDIR /root/or-tools

RUN make -j 8 third_party

RUN make -j 8 cc fz

RUN ln -s /root/or-tools/bin/fz /entry_data/fzn-exec

RUN cp /root/or-tools/ortools/flatzinc/mznlib_sat/*mzn /entry_data/mzn-lib