FROM ortools/make:opensuse_swig AS env

# .NET install
# see: https://docs.microsoft.com/en-us/dotnet/core/install/linux-opensuse
RUN zypper refresh \
&& zypper install -y wget tar awk gzip libicu-devel findutils

## .Net 8.0
# see: https://learn.microsoft.com/en-us/dotnet/core/install/linux-scripted-manual#scripted-install
RUN wget -q "https://dot.net/v1/dotnet-install.sh" \
&& chmod a+x dotnet-install.sh \
&& ./dotnet-install.sh -c 8.0 -i /usr/local/bin
# Trigger first run experience by running arbitrary cmd
RUN dotnet --info

FROM env AS devel
WORKDIR /home/project
COPY . .

FROM devel AS build
RUN make dotnet

FROM build AS test
RUN make test_dotnet

FROM build AS package
RUN make package_dotnet
