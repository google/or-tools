## Build GLPK with Microsoft Visual Studio Express ##

CFLAGS = \
/Iw32 \
/Isrc \
/Isrc\amd \
/Isrc\colamd \
/Isrc\minisat \
/Isrc\zlib \
/DHAVE_CONFIG_H=1 \
/D_CRT_SECURE_NO_WARNINGS=1 \
/nologo \
/W3 \
/O2 \
/MD

OBJSET = \
src/glpapi01.obj \
src/glpapi02.obj \
src/glpapi03.obj \
src/glpapi04.obj \
src/glpapi05.obj \
src/glpapi06.obj \
src/glpapi07.obj \
src/glpapi08.obj \
src/glpapi09.obj \
src/glpapi10.obj \
src/glpapi11.obj \
src/glpapi12.obj \
src/glpapi13.obj \
src/glpapi14.obj \
src/glpapi15.obj \
src/glpapi16.obj \
src/glpapi17.obj \
src/glpapi18.obj \
src/glpapi19.obj \
src/glpapi20.obj \
src/glpapi21.obj \
src/glpavl.obj \
src/glpbfd.obj \
src/glpbfx.obj \
src/glpcpx.obj \
src/glpdmp.obj \
src/glpdmx.obj \
src/glpenv01.obj \
src/glpenv02.obj \
src/glpenv03.obj \
src/glpenv04.obj \
src/glpenv05.obj \
src/glpenv06.obj \
src/glpenv07.obj \
src/glpenv08.obj \
src/glpfhv.obj \
src/glpgmp.obj \
src/glphbm.obj \
src/glpini01.obj \
src/glpini02.obj \
src/glpios01.obj \
src/glpios02.obj \
src/glpios03.obj \
src/glpios04.obj \
src/glpios05.obj \
src/glpios06.obj \
src/glpios07.obj \
src/glpios08.obj \
src/glpios09.obj \
src/glpios10.obj \
src/glpios11.obj \
src/glpios12.obj \
src/glpipm.obj \
src/glplib01.obj \
src/glplib02.obj \
src/glplib03.obj \
src/glplpf.obj \
src/glplpx01.obj \
src/glplpx02.obj \
src/glplpx03.obj \
src/glpluf.obj \
src/glplux.obj \
src/glpmat.obj \
src/glpmpl01.obj \
src/glpmpl02.obj \
src/glpmpl03.obj \
src/glpmpl04.obj \
src/glpmpl05.obj \
src/glpmpl06.obj \
src/glpmps.obj \
src/glpnet01.obj \
src/glpnet02.obj \
src/glpnet03.obj \
src/glpnet04.obj \
src/glpnet05.obj \
src/glpnet06.obj \
src/glpnet07.obj \
src/glpnet08.obj \
src/glpnet09.obj \
src/glpnpp01.obj \
src/glpnpp02.obj \
src/glpnpp03.obj \
src/glpnpp04.obj \
src/glpnpp05.obj \
src/glpnpp06.obj \
src/glpqmd.obj \
src/glprgr.obj \
src/glprlx.obj \
src/glprng01.obj \
src/glprng02.obj \
src/glpscf.obj \
src/glpscl.obj \
src/glpsdf.obj \
src/glpspm.obj \
src/glpspx01.obj \
src/glpspx02.obj \
src/glpsql.obj \
src/glpssx01.obj \
src/glpssx02.obj \
src/glptsp.obj \
src/amd/amd_1.obj \
src/amd/amd_2.obj \
src/amd/amd_aat.obj \
src/amd/amd_control.obj \
src/amd/amd_defaults.obj \
src/amd/amd_dump.obj \
src/amd/amd_info.obj \
src/amd/amd_order.obj \
src/amd/amd_post_tree.obj \
src/amd/amd_postorder.obj \
src/amd/amd_preprocess.obj \
src/amd/amd_valid.obj \
src/colamd/colamd.obj \
src/minisat/minisat.obj \
src/zlib/adler32.obj \
src/zlib/compress.obj \
src/zlib/crc32.obj \
src/zlib/deflate.obj \
src/zlib/gzclose.obj \
src/zlib/gzlib.obj \
src/zlib/gzread.obj \
src/zlib/gzwrite.obj \
src/zlib/infback.obj \
src/zlib/inffast.obj \
src/zlib/inflate.obj \
src/zlib/inftrees.obj \
src/zlib/trees.obj \
src/zlib/uncompr.obj \
src/zlib/zio.obj \
src/zlib/zutil.obj

.c.obj:
	cl.exe $(CFLAGS) /Fo$*.obj /c $*.c

all: glpk.lib glpsol.exe

glpk.lib: $(OBJSET)
	lib.exe /out:glpk.lib src\*.obj src\amd\*.obj src\colamd\*.obj src\minisat\*.obj src\zlib\*.obj

glpsol.exe: examples\glpsol.obj glpk.lib
	cl.exe $(CFLAGS) /Feglpsol.exe examples\glpsol.obj glpk.lib


