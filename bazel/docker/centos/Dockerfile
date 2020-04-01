# Create a virtual environment with all tools installed
# ref: https://hub.docker.com/_/centos
FROM centos:latest AS env
LABEL maintainer="corentinl@google.com"
# Install system build dependencies
ENV PATH=/usr/local/bin:$PATH
RUN yum -y update \
&& yum -y install git wget zlib-devel \
&& yum -y groupinstall "Development Tools" \
&& yum clean all \
&& rm -rf /var/cache/yum
# Install bazel
# see: https://docs.bazel.build/versions/master/install-redhat.html#installing-on-centos-7
RUN cd /etc/yum.repos.d \
&& wget https://copr.fedorainfracloud.org/coprs/vbatts/bazel/repo/epel-8/vbatts-bazel-epel-8.repo \
&& yum -y update \
&& yum -y install bazel \
&& yum clean all \
&& rm -rf /var/cache/yum
CMD [ "/usr/bin/bash" ]

FROM env AS devel
WORKDIR /home/lib
COPY . .

FROM devel as build
RUN bazel build --curses=no --copt='-Wno-sign-compare' //...:all

FROM build as test
RUN bazel test -c opt --curses=no --copt='-Wno-sign-compare' //...:all
