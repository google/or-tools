main_dir=$2

# List all files on ortools/$main_dir
all_cc=`ls ortools/$main_dir/*.cc | grep -v test.cc`
all_h=`ls ortools/$main_dir/*.h`
if ls ortools/$main_dir/*proto 1> /dev/null 2>&1; then
    all_proto=`ls ortools/$main_dir/*.proto`
else
    all_proto=
fi

# Utility functions
function src_or_gen_dir {
    if [[ $1 == *\.pb\.* ]]
    then
        echo \$\(GEN_DIR\)/$1
    else
        echo \$\(SRC_DIR\)/$1
    fi
}

function print_include_list {
    all_deps=($1)
    last_dep=${all_deps[@]:(-1)}
    for dep in "${all_deps[@]}"
    do
        if [[ $dep == $last_dep ]]
        then
            echo \ \ \ \ $(src_or_gen_dir $dep)
        else
            echo \ \ \ \ $(src_or_gen_dir $dep) \\
        fi
    done
}

function print_list_with_prefix {
    all_files=($1)
    last_file="${all_files[@]:(-1)}"
    for file in "${all_files[@]}"
    do
        if [[ $file == $last_file ]]
        then
            echo "$2"$file
        else
            echo "$2"$file \\
        fi
    done
}

# Generate XXX_DEPS macro
all_deps_str=
for dir in ${@:2}
do
    all_deps_str+=\ $(grep -e "^\#include \"ortools/$dir" ortools/$dir/*.h | cut -d '"' -f 2 | sort -u)
done
echo $1_DEPS = \\
print_include_list "$all_deps_str"
echo

# generate XXX_LIB_OBJS macro.
# Get list of obj files to build.
all_cc_objs_tmp=${all_cc//ortools/\$\(OBJ_DIR\)}
all_cc_objs=${all_cc_objs_tmp//\.cc/\.\$O}
all_proto_objs_tmp=${all_proto//ortools/\$\(OBJ_DIR\)}
all_proto_objs=${all_proto_objs_tmp//\.proto/.pb.\$O}
all_objs=$all_cc_objs
all_objs+=\ $all_proto_objs
# Print makefile macro definition.
echo $1_LIB_OBJS = \\
print_list_with_prefix "$all_objs" "    "
echo

# Generate dependencies for .h files
for file in $all_h
do
    name=`basename $file .h`
    # Compute dependencies.
    all_deps_str=
    for dir in ${@:2}
    do
        all_deps_str+=\ $(grep -e "^\#include \"ortools/$dir" $file | cut -d '"' -f 2 | sort -u)
    done
    # Print makefile command.
    if [[ ! -z ${all_deps_str// } ]]
    then
        echo \$\(SRC_DIR\)/ortools/$2/$name.h: \\
        print_include_list "$all_deps_str"
        echo
    fi
done

# Generate dependencies and compilation command for .cc files.
for file in $all_cc
do
    name=`basename $file .cc`
    # Compute dependencies.
    all_deps_str=
    for dir in ${@:2}
    do
        all_deps_str+=\ $(grep -e "^\#include \"ortools/$dir" $file | cut -d '"' -f 2 | sort -u)
    done
    # Print makefile command.
    echo \$\(OBJ_DIR\)/$2/$name.\$O: \\
    echo \ \ \ \ \$\(SRC_DIR\)/ortools/$2/$name.cc \\
    print_include_list "$all_deps_str"
    echo -e '\t'\$\(CCC\) \$\(CFLAGS\) -c \$\(SRC_DIR\)\$Sortools\$S$2\$S$name.cc \$\(OBJ_OUT\)\$\(OBJ_DIR\)\$S$2$\S$name.\$O
    echo
done

# Generate dependencies, compulation, and protoc command for .proto files.
for file in $all_proto
do
    name=`basename $file .proto`
    # Compute inter proto dependencies.
    all_deps_str=
    for dir in ${@:2}
    do
        all_deps_str+=\ $(grep -e "^\import \"ortools/$dir" $file | cut -d '"' -f 2 | sed -e "s/proto/pb.h/" | sort -u)
    done
    # Print makefile command.
    echo \$\(GEN_DIR\)/ortools/$2/$name.pb.cc: \$\(SRC_DIR\)/ortools/$2/$name.proto
    echo -e '\t'\$\(PROTOBUF_DIR\)/bin/protoc --proto_path=\$\(INC_DIR\) --cpp_out=\$\(GEN_DIR\) \$\(SRC_DIR\)/ortools/$2/$name.proto
    echo
    if [[ ! -z ${all_deps_str// } ]]
    then
        echo \$\(GEN_DIR\)/ortools/$2/$name.pb.h: \$\(GEN_DIR\)/ortools/$2/$name.pb.cc \\
        print_include_list "$all_deps_str"
    else
        echo \$\(GEN_DIR\)/ortools/$2/$name.pb.h: \$\(GEN_DIR\)/ortools/$2/$name.pb.cc
    fi
    echo
    echo \$\(OBJ_DIR\)/$2/$name.pb.\$O: \$\(GEN_DIR\)/ortools/$2/$name.pb.cc
    echo -e '\t'\$\(CCC\) \$\(CFLAGS\) -c \$\(GEN_DIR\)/ortools/$2/$name.pb.cc \$\(OBJ_OUT\)\$\(OBJ_DIR\)\$S$2$\S$name.pb.\$O
    echo
done
