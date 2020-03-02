FROM quay.io/pypa/manylinux2010_x86_64:latest
LABEL maintainer="mizux.dev@gmail.com"

# Base install
ENV PATH=$PATH:/usr/local/bin
RUN yum -y update \
&& yum -y groupinstall 'Development Tools' \
&& yum -y install wget pcre-devel which redhat-lsb-core \
&& yum clean all \
&& rm -rf /var/cache/yum
# Install CMake 3.16.4
RUN wget "https://cmake.org/files/v3.16/cmake-3.16.4-Linux-x86_64.sh" \
&& chmod a+x cmake-3.16.4-Linux-x86_64.sh \
&& ./cmake-3.16.4-Linux-x86_64.sh --prefix=/usr/local/ --skip-license \
&& rm cmake-3.16.4-Linux-x86_64.sh
CMD [ "/bin/sh" ]

# CMake version
RUN cmake -version

# Swig install
RUN wget "https://downloads.sourceforge.net/project/swig/swig/swig-4.0.1/swig-4.0.1.tar.gz" \
&& tar xvf swig-4.0.1.tar.gz \
&& rm swig-4.0.1.tar.gz \
&& cd swig-4.0.1 \
&& ./configure --prefix=/usr \
&& make -j 4 \
&& make install \
&& cd .. \
&& rm -rf swig-4.0.1

COPY build_manylinux.sh .
RUN chmod a+x build_manylinux.sh
