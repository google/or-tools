# Create a virtual environment with all tools installed
# ref: https://hub.docker.com/r/opensuse/leap
FROM opensuse/leap

# Install system build dependencies
ENV PATH=/usr/local/bin:$PATH
RUN zypper refresh \
&& zypper install -y make \
&& zypper clean -a
ENV CC=gcc-11 CXX=g++-11
ENTRYPOINT ["/usr/bin/bash", "-c"]
CMD ["/usr/bin/bash"]

# Python Install
RUN zypper install -y python3-devel python3-pip python3-wheel \
&& zypper clean -a
RUN python3 -m pip install absl-py mypy-protobuf

WORKDIR /root
ADD or-tools_amd64_opensuse-leap_python_v*.tar.gz .

RUN cd or-tools_*_v* && make test
