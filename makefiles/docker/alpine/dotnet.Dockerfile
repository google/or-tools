FROM ortools/make:alpine_swig AS env

# .NET install
RUN apk add --no-cache dotnet8-sdk
# Trigger first run experience by running arbitrary cmd
RUN dotnet --info

# Add the library src to our build env
FROM env AS devel
WORKDIR /home/project
COPY . .

FROM devel AS build
RUN make dotnet

FROM build AS test
RUN make test_dotnet

FROM build AS package
RUN make package_dotnet
