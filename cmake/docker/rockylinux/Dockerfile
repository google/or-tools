# Create a virtual environment with all tools installed
# ref: https://hub.docker.com/_/rockylinux
FROM rockylinux:9 AS base

# Install system build dependencies
ENV PATH=/usr/local/bin:$PATH
RUN dnf -y update \
&& dnf -y install git wget openssl-devel cmake \
&& dnf -y group install "Development Tools" \
&& dnf clean all \
&& rm -rf /var/cache/dnf
CMD [ "/usr/bin/bash" ]

# Install SWIG 4.2.1
FROM base AS swig
RUN dnf -y update \
&& dnf -y install pcre2-devel \
&& dnf clean all \
&& rm -rf /var/cache/dnf \
&& wget -q "https://downloads.sourceforge.net/project/swig/swig/swig-4.2.1/swig-4.2.1.tar.gz" \
&& tar xvf swig-4.2.1.tar.gz \
&& rm swig-4.2.1.tar.gz \
&& cd swig-4.2.1 \
&& ./configure --prefix=/usr \
&& make -j 4 \
&& make install \
&& cd .. \
&& rm -rf swig-4.2.1
