# This script must be run when you uncompress the archive or if you move it to
# a new position.

export N=`pwd`/lib
echo install library in path $N
install_name_tool -id $N/libortools.dylib lib/libortools.dylib
export P=`otool -L lib/libortools.dylib | grep -v ':' | grep libortools | cut -d '(' -f 1`
echo the library was installed in $P
