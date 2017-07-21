
export N=`pwd`/lib
export B=`pwd`/bin
export O=`otool -L lib/libortools.dylib | grep -v ':' | grep libortools | cut -d '(' -f 1`
echo install library in path $N
echo "$O"
if [ -e $B/libGoogle.OrTools.so ]
then
    install_name_tool -change $O @loader_path/../../lib/libortools.dylib $B/libGoogle.OrTools.so
fi
install_name_tool -id @rpath/libortools.dylib $N/libortools.dylib
if [ -e $N/libjniortools.dylib ]
then
    install_name_tool -change $O @loader_path/libortools.dylib $N/libjniortools.dylib
fi
install_name_tool -id @rpath/libfap.dylib $N/libfap.dylib
install_name_tool -id @rpath/libdimacs.dylib $N/libdimacs.dylib
install_name_tool -id @rpath/libcvrptw_lib.dylib $N/libcvrptw_lib.dylib
if [ -e $N/libjniortools.jnilib ]
then
    install_name_tool -change $O @loader_path/libortools.dylib $N/libjniortools.jnilib
fi
export P=`otool -L lib/libortools.dylib | grep -v ':' | grep libortools | cut -d '(' -f 1`
echo the new path of the or-tools library is $P
