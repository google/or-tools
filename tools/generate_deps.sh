deps_decl=$1
main_dir=$2

for i in src/$main_dir/*.h
do
    file=`basename $i .h`
    line=\$\(SRC_DIR\)/$main_dir/$file.h:
    for dir in ${@:2}
    do
        for deps in `grep -e "\#include \"$dir" $i | cut -d '"' -f 2`
        do
            if [[ $deps == *pb.h ]]
            then
                line=$line\ \$\(GEN_DIR\)/$deps
            else
                line=$line\ \$\(SRC_DIR\)/$deps
            fi
        done
    done
    echo
    echo $line
done

for i in src/$main_dir/*.cc
do
    file=`basename $i .cc`
    line=\$\(OBJ_DIR\)/$main_dir/$file.\$O:
    for deps in `grep -e "\#include \"$main_dir" $i | cut -d '"' -f 2`
    do
        if [[ $deps == *pb.h ]]
        then
            line=$line\ \$\(GEN_DIR\)/$deps
        else
            line=$line\ \$\(SRC_DIR\)/$deps
        fi
    done
    echo
    echo $line \$\($deps_decl\)
    echo -e '\t'\$\(CCC\) \$\(CFLAGS\) -c \$\(SRC_DIR\)/$main_dir/$file.cc \$\(OBJ_OUT\)\$\(OBJ_DIR\)\$S$main_dir$\S$file.\$O
done

for i in src/$main_dir/*proto
do
    file=`basename $i .proto`
    echo
    echo \$\(GEN_DIR\)/$main_dir/$file.pb.cc: \$\(SRC_DIR\)/$main_dir/$file.proto
    echo -e '\t'\$\(PROTOBUF_DIR\)/bin/protoc --proto_path=\$\(INC_DIR\) --cpp_out=\$\(GEN_DIR\) \$\(SRC_DIR\)/$main_dir/$file.proto
    echo
    echo \$\(GEN_DIR\)/$main_dir/$file.pb.h: \$\(GEN_DIR\)/$main_dir/$file.pb.cc
    echo
    echo \$\(OBJ_DIR\)/$main_dir/$file.pb.\$O: \$\(GEN_DIR\)/$main_dir/$file.pb.cc \$\($deps_decl\)
    echo -e '\t'\$\(CCC\) \$\(CFLAGS\) -c \$\(GEN_DIR\)/$main_dir/$file.pb.cc \$\(OBJ_OUT\)\$\(OBJ_DIR\)\$S$main_dir$\S$file.pb.\$O
done
