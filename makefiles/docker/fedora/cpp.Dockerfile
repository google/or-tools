FROM ortools/make:fedora_base AS env
RUN make -version

FROM env AS devel
WORKDIR /home/project
COPY . .

FROM devel AS build
RUN make third_party
RUN make cc

FROM build AS test
RUN make test_cc

FROM build AS package
RUN make package_cc
