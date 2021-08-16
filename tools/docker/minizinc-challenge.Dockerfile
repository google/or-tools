FROM minizinc/mznc2021:latest

ENV SRC_GIT_BRANCH master

ENV TZ=America/Los_Angeles
RUN ln -snf /usr/share/zoneinfo/$TZ /etc/localtime && echo $TZ > /etc/timezone

RUN apt-get update

RUN apt-get -y install pkg-config git wget autoconf libtool zlib1g-dev gawk g++ curl cmake make lsb-release python-dev gfortran gcc-8

WORKDIR /root

RUN git clone -b "$SRC_GIT_BRANCH" --single-branch https://github.com/google/or-tools

WORKDIR /root/or-tools

RUN make Makefile.local

RUN echo USE_SCIP=OFF >> Makefile.local

RUN echo USE_COINOR=OFF >> Makefile.local

RUN mkdir ortools/gen

RUN mkdir ortools/gen/ortools

RUN mkdir ortools/gen/ortools/linear_solver

RUN touch ortools/gen/ortools/linear_solver/lpi_glop.cc

RUN make -j 4 third_party

RUN make -j 4 cc fz

RUN ln -s /root/or-tools/bin/fz /entry_data/fzn-exec

RUN cp /root/or-tools/ortools/flatzinc/mznlib/*mzn /entry_data/mzn-lib