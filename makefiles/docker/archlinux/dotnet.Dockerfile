FROM ortools/make:archlinux_swig AS env
RUN pacman -Syu --noconfirm dotnet-sdk
# Trigger first run experience by running arbitrary cmd
RUN dotnet --info

FROM env AS devel
WORKDIR /home/project
COPY . .

FROM devel AS build
ENV USE_DOTNET_CORE_31=OFF
RUN make dotnet

FROM build AS test
RUN make test_dotnet

FROM build AS package
RUN make package_dotnet
