# Create a virtual environment with all tools installed
# ref: https://hub.docker.com/_/archlinux/
FROM archlinux:latest AS base

# Install system build dependencies
ENV PATH=/usr/local/bin:$PATH
RUN pacman -Syu --noconfirm git base-devel cmake

# Get all dependencies from the package manager
RUN pacman -Syu --noconfirm \
 abseil-cpp protobuf re2 eigen \
 coin-or-cbc coin-or-clp \
 glpk \
 scip \
 gtest benchmark

ENTRYPOINT ["/bin/bash", "-c"]
CMD [ "/bin/bash" ]

FROM base AS swig
RUN pacman -Syu --noconfirm swig
