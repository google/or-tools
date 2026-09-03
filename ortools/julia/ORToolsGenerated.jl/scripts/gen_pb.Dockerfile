# ref: https://hub.docker.com/_/debian
FROM debian:12

RUN apt-get update \
&& apt-get install -yq wget \
&& apt-get clean \
&& rm -rf /var/lib/apt/lists/* /tmp/* /var/tmp/*
ENTRYPOINT ["/bin/bash", "-c"]
CMD ["/bin/bash"]

ENV JULIA_PATH /usr/local/julia
ENV PATH $JULIA_PATH/bin:$PATH
RUN arch="$(dpkg --print-architecture)"; \
    case "$arch" in \
        'amd64') \
            url='https://julialang-s3.julialang.org/bin/linux/x64/1.10/julia-1.10.10-linux-x86_64.tar.gz'; \
            ;; \
        'arm64') \
            url='https://julialang-s3.julialang.org/bin/linux/aarch64/1.10/julia-1.10.10-linux-aarch64.tar.gz'; \
            ;; \
        *) \
            echo >&2 "error: architecture ($arch) has no Julia binary available"; \
            exit 1; \
            ;; \
    esac; \
    wget "$url" -O julia.tar.gz; \
    mkdir "$JULIA_PATH"; \
    tar -xzf julia.tar.gz -C "$JULIA_PATH" --strip-components 1; \
    rm julia.tar.gz; \
    julia --version

WORKDIR /workspace
