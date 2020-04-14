FROM ortools/make:centos_swig AS env
# see https://docs.microsoft.com/en-us/dotnet/core/install/linux-package-manager-centos7
RUN rpm -Uvh "https://packages.microsoft.com/config/centos/7/packages-microsoft-prod.rpm" \
&& yum -y update \
&& yum -y install dotnet-sdk-3.1 \
&& yum clean all \
&& rm -rf /var/cache/yum
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
