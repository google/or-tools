FROM ortools/make:fedora_swig AS env
# see: https://docs.microsoft.com/en-us/dotnet/core/install/linux-package-manager-fedora31
RUN rpm --import https://packages.microsoft.com/keys/microsoft.asc \
&& wget -q -O /etc/yum.repos.d/microsoft-prod.repo https://packages.microsoft.com/config/fedora/31/prod.repo \
&& dnf -y update \
&& dnf -y install dotnet-sdk-3.1 \
&& dnf clean all
# Trigger first run experience by running arbitrary cmd
RUN dotnet --info

FROM env AS devel
WORKDIR /home/lib
COPY . .

FROM devel AS build
RUN make third_party
RUN make dotnet

FROM build AS test
RUN make test_dotnet

FROM build AS package
RUN make package_dotnet
