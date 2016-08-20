export or=`otool -L bin/fzn-or-tools | grep -v ':' | grep libortools | cut -d '(' -f 1`
export fz=`otool -L bin/fzn-or-tools | grep -v ':' | grep libfz | cut -d '(' -f 1`

install_name_tool -change $or @loader_path/../lib/libortools.dylib bin/fzn-or-tools
install_name_tool -change $fz @loader_path/../lib/libfz.dylib bin/fzn-or-tools

export new_or=`otool -L bin/fzn-or-tools | grep -v ':' | grep libortools | cut -d '(' -f 1`
echo the new path of or-tools library is $new_or
export new_fz=`otool -L bin/fzn-or-tools | grep -v ':' | grep libfz | cut -d '(' -f 1`
echo the new path of fz library is $new_f