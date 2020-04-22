FROM ortools/cmake:centos_swig AS env
# Currently centos package is broken
# see: https://developercommunity.visualstudio.com/content/problem/990297/centos-8-dotnet-sdk-dont-provide-the-sdk.html
## see https://docs.microsoft.com/en-us/dotnet/core/install/linux-package-manager-centos7
#RUN rpm -Uvh "https://packages.microsoft.com/config/centos/7/packages-microsoft-prod.rpm" \
#&& yum -y update \
#&& yum -y install dotnet-sdk-3.1 \
#&& yum clean all \
#&& rm -rf /var/cache/yum
RUN wget "https://dotnet.microsoft.com/download/dotnet-core/scripts/v1/dotnet-install.sh" \
&& chmod a+x dotnet-install.sh \
&& ./dotnet-install.sh \
&& rm dotnet-install.sh
ENV PATH=/root/.dotnet:$PATH
# Trigger first run experience by running arbitrary cmd
RUN dotnet --info

FROM env AS devel
WORKDIR /home/project
COPY . .

FROM devel AS build
RUN cmake -S. -Bbuild -DBUILD_DEPS=ON -DBUILD_DOTNET=ON
RUN cmake --build build --target all
RUN cmake --build build --target install

FROM build AS test
RUN cmake --build build --target test

FROM env AS install_env
WORKDIR /home/sample
COPY --from=build /home/project/build/dotnet/packages/*.nupkg ./

FROM install_env AS install_devel
COPY cmake/samples/dotnet .

FROM install_devel AS install_build
RUN dotnet build

FROM install_build AS install_test
RUN dotnet test
