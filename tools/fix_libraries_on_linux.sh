dependencies/install/bin/patchelf --set-rpath '$ORIGIN' temp/or-tools*/lib/libjniortools.so
dependencies/install/bin/patchelf --set-rpath '$ORIGIN' temp/or-tools*/lib/libcvrptw_lib.so
dependencies/install/bin/patchelf --set-rpath '$ORIGIN' temp/or-tools*/lib/libdimacs.so
dependencies/install/bin/patchelf --set-rpath '$ORIGIN/../../lib' temp/or-tools*/bin/libGoogle.OrTools.so
