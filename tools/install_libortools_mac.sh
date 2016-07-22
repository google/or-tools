# This script must be run when you uncompress the archive or if you move it to
# a new position.

export N=`pwd`/lib
export O=`otool -L lib/libortools.dylib | grep -v ':' | grep libortools | cut -d '(' -f 1`
echo the old path of the or-tools library is $O
echo install library in path $N
install_name_tool -id $N/libGoogle.OrTools.so lib/libGoogle.OrTools.so
install_name_tool -change $O $N/libortools.dylib lib/libGoogle.OrTools.so
install_name_tool -id $N/libcvrptw_lib.dylib lib/libcvrptw_lib.dylib
install_name_tool -id $N/libdimacs.dylib lib/libdimacs.dylib
install_name_tool -id $N/libjniortools.jnilib lib/libjniortools.jnilib
install_name_tool -change $O $N/libortools.dylib lib/libjniortools.jnilib
install_name_tool -id $N/libortools.dylib lib/libortools.dylib
export P=`otool -L lib/libortools.dylib | grep -v ':' | grep libortools | cut -d '(' -f 1`
echo the new path of the or-tools library is $P
