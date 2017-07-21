export N=`pwd`/lib
export B=`pwd`/bin
export O=`otool -L lib/libortools.dylib | grep -v ':' | grep libortools | cut -d '(' -f 1`

echo install or-tools libraries in path $N
echo the current path of the or-tools library is $O

install_name_tool -id @rpath/libortools.dylib $N/libortools.dylib

if [ -e $B/libGoogle.OrTools.so ]
then
  install_name_tool -change $O @loader_path/../../lib/libortools.dylib $B/libGoogle.OrTools.so
fi

if [ -e $N/libjniortools.dylib ]
then
    install_name_tool -change $O @loader_path/libortools.dylib $N/libjniortools.dylib
fi

if [ -e $N/libfap.dylib ]
then
    install_name_tool -id @rpath/libfap.dylib $N/libfap.dylib
fi

if [ -e $N/libdimacs.dylib ]
then
  install_name_tool -id @rpath/libdimacs.dylib $N/libdimacs.dylib
fi

if [ -e $N/libcvrptw_lib.dylib ]
then
  install_name_tool -id @rpath/libcvrptw_lib.dylib $N/libcvrptw_lib.dylib
fi

if [ -e $N/libjniortools.jnilib ]
then
    install_name_tool -change $O @loader_path/libortools.dylib $N/libjniortools.jnilib
fi

export P=`otool -L lib/libortools.dylib | grep -v ':' | grep libortools | cut -d '(' -f 1`
echo the new path of the or-tools library is $P
