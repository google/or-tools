FROM centos:8
LABEL maintainer="corentinl@google.com"

RUN yum -y update \
&& yum -y groupinstall 'Development Tools' \
&& yum -y install zlib-devel \
&& yum clean all \
&& rm -rf /var/cache/yum

# Install dotnet
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

#ENV TZ=America/Los_Angeles
#RUN ln -snf /usr/share/zoneinfo/$TZ /etc/localtime && echo $TZ > /etc/timezone

WORKDIR /root
ADD or-tools_centos-8_v*.tar.gz .

RUN cd or-tools_*_v* && make test_dotnet
