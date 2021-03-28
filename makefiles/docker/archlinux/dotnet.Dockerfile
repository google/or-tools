FROM ortools/make:archlinux_swig AS env
# Currently dotnet-sdk (.Net 5) is broken
# see: https://github.com/google/or-tools/issues/2465
RUN pacman -Syu --noconfirm dotnet-sdk-3.1
# Trigger first run experience by running arbitrary cmd
RUN dotnet --info

FROM env AS devel
WORKDIR /home/project
COPY . .

FROM devel AS build
RUN make third_party
RUN make dotnet

FROM build AS test
RUN make test_dotnet

FROM build AS package
RUN make package_dotnet
