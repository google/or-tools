# Create a virtual environment with all tools installed
# ref: https://hub.docker.com/_/archlinux/
FROM archlinux:latest AS env

#############
##  SETUP  ##
#############
ENV PATH=/usr/local/bin:$PATH
RUN pacman -Syu --noconfirm make
ENTRYPOINT ["/bin/bash", "-c"]
CMD [ "/bin/bash" ]

# Install Java
RUN pacman -Syu --noconfirm jdk-openjdk maven
ENV JAVA_HOME=/usr/lib/jvm/default

WORKDIR /root
ADD or-tools_amd64_archlinux_java_v*.tar.gz .

RUN cd or-tools_*_v* && make test
