# This script must be run when you uncompress the archive or if you move it to
# a new position.

export N=`pwd`/lib
export B=`pwd`/bin/csharp
export O=`otool -L lib/libortools.dylib | grep -v ':' | grep libortools | cut -d '(' -f 1`
echo install library in path $N
install_name_tool -id $B/libGoogle.OrTools.so $B/libGoogle.OrTools.so
install_name_tool -change $O $N/libortools.dylib $B/libGoogle.OrTools.so
install_name_tool -id $N/libcvrptw_lib.dylib $N/libcvrptw_lib.dylib
install_name_tool -id $N/libdimacs.dylib $N/libdimacs.dylib
install_name_tool -id $N/libjniortools.jnilib $N/libjniortools.jnilib
install_name_tool -change $O $N/libortools.dylib $N/libjniortools.jnilib
install_name_tool -id $N/libortools.dylib $N/libortools.dylib
export P=`otool -L lib/libortools.dylib | grep -v ':' | grep libortools | cut -d '(' -f 1`
echo the new path of the or-tools library is $P
