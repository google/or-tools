export build=`pwd`/dependencies/install
export source=`pwd`/dependencies/sources
export archive=`pwd`/dependencies/archives
mkdir -p $build

cd $archive
curl -OL http://ftpmirror.gnu.org/autoconf/autoconf-2.68.tar.gz
cd $source
tar xzf $archive/autoconf-2.68.tar.gz
cd autoconf-2.68
./configure --prefix=$build
make
make install
export PATH=$PATH:$build/bin

cd $archive
curl -OL http://ftpmirror.gnu.org/automake/automake-1.11.tar.gz
cd $source
tar xzf $archive/automake-1.11.tar.gz
cd automake-1.11
./configure --prefix=$build
make
make install

cd $archive
curl -OL http://ftpmirror.gnu.org/libtool/libtool-2.4.tar.gz
cd $source
tar xzf $archive/libtool-2.4.tar.gz
cd libtool-2.4
./configure --prefix=$build
make
make install