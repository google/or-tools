FROM ubuntu:14.04

RUN apt-get update

RUN apt-get -y install wget

RUN wget "http://keyserver.ubuntu.com/pks/lookup?op=get&search=0x3FA7E0328081BFF6A14DA29AA6A19B38D3D831EF" -O out && apt-key add out && rm out

RUN echo "deb http://download.mono-project.com/repo/debian wheezy main" | sudo tee /etc/apt/sources.list.d/mono-xamarin.list

RUN apt-get update

RUN apt-get -y install git autoconf libtool zlib1g-dev gawk g++ curl subversion make mono-complete lsb-release python-dev default-jdk python-setuptools python-six python3-setuptools python3-dev libpcre3-dev python-wheel python3-wheel

WORKDIR /root

RUN wget "https://cmake.org/files/v3.8/cmake-3.8.2-Linux-x86_64.sh"

RUN chmod 775 cmake-3.8.2-Linux-x86_64.sh

RUN yes | ./cmake-3.8.2-Linux-x86_64.sh --prefix=/usr --exclude-subdir

RUN wget "https://downloads.sourceforge.net/project/swig/swig/swig-3.0.12/swig-3.0.12.tar.gz"

RUN tar xvf swig-3.0.12.tar.gz

WORKDIR /root/swig-3.0.12

RUN ./configure --prefix=/usr

RUN make -j 4

RUN make install

WORKDIR /root

RUN git clone https://github.com/google/or-tools

WORKDIR /root/or-tools

RUN make third_party
