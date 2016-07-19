main_dir=$2

# Generate XXX_DEPS macro
echo $1_DEPS= \\
for dir in ${@:2}
do
    for deps in `grep -e "\#include \"$dir" src/$dir/*.h | cut -d '"' -f 2 | sort -u`
    do
        if [[ $deps == *pb.h ]]
        then
            echo \ \ \ \ \$\(GEN_DIR\)/$deps \\
        else
            echo \ \ \ \ \$\(SRC_DIR\)/$deps \\
        fi
    done
done
echo
echo

# generate XXX_LIB_OBJS macro
echo $1_LIB_OBJS = \\
for i in src/$2/*.cc
do
    file=`basename $i .cc`
    echo \ \ \ \ \$\(OBJ_DIR\)/$2/$file.\$O: \\
done
for i in src/$2/*.proto
do
    file=`basename $i .proto`
    echo \ \ \ \ \$\(OBJ_DIR\)/$2/$file.pb.\$O \\
done
echo
echo

# Generate dependencies for .h files
for i in src/$2/*.h
do
    file=`basename $i .h`
    echo \$\(SRC_DIR\)/$2/$file.h: \\
    for dir in ${@:2}
    do
        for deps in `grep -e "\#include \"$dir" $i | cut -d '"' -f 2`
        do
            if [[ $deps == *pb.h ]]
            then
                echo \ \ \ \ \$\(GEN_DIR\)/$deps \\
            else
                echo \ \ \ \ \$\(SRC_DIR\)/$deps \\
            fi
        done
    done
    echo
    echo
done

# Generate dependencies and compilation command for .cc files
for i in src/$2/*.cc
do
    file=`basename $i .cc`
    echo \$\(OBJ_DIR\)/$2/$file.\$O: \\
    for dir in ${@:2}
    do
        for deps in `grep -e "\#include \"$dir" $i | cut -d '"' -f 2`
        do
            if [[ $deps == *pb.h ]]
            then
                echo \ \ \ \ \$\(GEN_DIR\)/$deps \\
            else
                echo \ \ \ \ \$\(SRC_DIR\)/$deps \\
            fi
        done
    done
    echo
    echo -e '\t'\$\(CCC\) \$\(CFLAGS\) -c \$\(SRC_DIR\)/$2/$file.cc \$\(OBJ_OUT\)\$\(OBJ_DIR\)\$S$2$\S$file.\$O
    echo
done

# Generate dependencies, compulation, and protoc command for .proto files.
for i in src/$2/*proto
do
    file=`basename $i .proto`
    echo
    echo \$\(GEN_DIR\)/$2/$file.pb.cc: \$\(SRC_DIR\)/$2/$file.proto
    echo -e '\t'\$\(PROTOBUF_DIR\)/bin/protoc --proto_path=\$\(INC_DIR\) --cpp_out=\$\(GEN_DIR\) \$\(SRC_DIR\)/$2/$file.proto
    echo
    echo \$\(GEN_DIR\)/$2/$file.pb.h: \$\(GEN_DIR\)/$2/$file.pb.cc
    echo
    echo \$\(OBJ_DIR\)/$2/$file.pb.\$O: \$\(GEN_DIR\)/$2/$file.pb.cc
    echo -e '\t'\$\(CCC\) \$\(CFLAGS\) -c \$\(GEN_DIR\)/$2/$file.pb.cc \$\(OBJ_OUT\)\$\(OBJ_DIR\)\$S$2$\S$file.pb.\$O
    echo
done

#TODO: Generate inter-proto dependencies.
