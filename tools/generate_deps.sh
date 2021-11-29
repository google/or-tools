#!/bin/bash
# usage: ./tools/generate_deps.sh BASE base
declare -r libname="${1}"
declare -r main_dir="${2}"

# List all files on ortools/"${main_dir}"
all_cc=( $(ls ortools/"${main_dir}"/*.cc ortools/gen/ortools/"${main_dir}"/lpi_glop*.cc | grep -v test.cc | LC_COLLATE=C sort -u) )
all_h=( $(ls ortools/"${main_dir}"/*.h | LC_COLLATE=C sort -u) )
declare -a all_proto
if ls ortools/"${main_dir}"/*proto >& /dev/null; then
  all_proto+=( $(ls ortools/"${main_dir}"/*.proto | LC_COLLATE=C sort -u) )
fi

# Arguments: a list of dependencies.
# Output: one line per dependency, listing its actual path in ortools,
#         as understood by the Makefile (eg. using $(SRC_DIR) etc).
#         Each line is appended with an ending '\', except the last one.
#
# Many kinds of input dependencies are supported:
# - Standard files, like like "ortools/base/stringprintf.h": those will
#   be output with the $(SRC_DIR)/ directory prefix.
# - Generated proto file *.pb.{h,cc}: those will be output with the
#   $(GEN_DIR)/ prefix
# - Object files *.$O: those will be output with the $(OBJ_DIR)/ prefix.
function print_paths {
  is_first_line=1
  for dep in "$@"; do
    ((is_first_line)) || echo " \\"
    is_first_line=0
    local dir="\$(SRC_DIR)/"
    [[ "${dep}" == *.pb.* ]] && dir="\$(GEN_DIR)/"
    if [[ "${dep}" == *.\$O ]]; then
      dir="\$(OBJ_DIR)/"
      dep="${dep/ortools\/gen\/}"  # Remove the "ortools/gen/" directory.
      dep="${dep/ortools\/}"  # Remove the "ortools/" directory.
    fi
    echo -e -n " ${dir}${dep}"
  done
}

# Input: a list of file names (eg. "ortools/base/file.h").
# Output: all the files these files depend on (given by their #include,
#         by their "import" for proto files).
function get_dependencies {
   grep -e "^\(#include\|import\) \"ortools/" "$*" \
     | cut -d '"' -f 2 | LC_COLLATE=C sort -u
}

# Input: filename sub_dir
# Output: dependencies command for that file:
#    objs/sub_dir/filename.o : ortools/
function print_dependencies {
  if [[ -e "ortools/${2}/${1}.cc" ]]; then
    local path="ortools/${2}/${1}.cc"
  elif [[ -e "ortools/gen/ortools/${2}/${1}.cc" ]]; then
    local path="ortools/gen/ortools/${2}/${1}.cc"
  fi
  cmd=$(gcc -std=c++17 -MM -MT "objs/${2}/${1}.o" -c "${path}" -I. -Iortools/gen \
            -isystem dependencies/install/include \
            -isystem dependencies/install/include/coin \
            -DUSE_GLOP -DUSE_BOP -DUSE_CLP -DUSE_CBC -DUSE_SCIP \
            | sed -e "s/\.o:/.\$O:/g")
  echo "${cmd} | \$(OBJ_DIR)/${2}"
}

# Generate XXX_DEPS macro
echo "${libname}"_DEPS = \\
print_paths ortools/"${main_dir}"/*.h "${all_proto[@]/.proto/.pb.h}"
echo ""
echo

# generate XXX_LIB_OBJS macro.
echo "${1}_LIB_OBJS = \\"
print_paths "${all_cc[@]/.cc/.\$O}" "${all_proto[@]/.proto/.pb.\$O}"
echo ""
echo

# Generate dependencies and compilation command for .cc files.
for file in "${all_cc[@]}"
do
  name=$(basename "${file}" .cc)
  # Compute dependencies.
  print_dependencies "${name}" "${main_dir}"
  if [[ -e "ortools/${main_dir}/${name}.cc" ]]; then
    echo -e "\t\$(CCC) \$(CFLAGS) -c \$(SRC_DIR)\$Sortools\$S${main_dir}\$S${name}.cc \$(OBJ_OUT)\$(OBJ_DIR)\$S${main_dir}\$S${name}.\$O"
  elif [[ -e "ortools/gen/ortools/${main_dir}/${name}.cc" ]]; then
    echo -e "\t\$(CCC) \$(CFLAGS) -c \$(GEN_PATH)\$Sortools\$S${main_dir}\$S${name}.cc \$(OBJ_OUT)\$(OBJ_DIR)\$S${main_dir}\$S${name}.\$O"
  fi
  echo
done

# Generate dependencies, compilation, and protoc command for .proto files.
for file in "${all_proto[@]}"
do
  name=$(basename "${file}" .proto)
  # Print makefile command for .proto.
  echo "ortools/${main_dir}/${name}.proto: ;"
  echo
  # Print makefile command for .pb.cc.
  echo -e "\$(GEN_DIR)/ortools/${main_dir}/${name}.pb.cc: \\"
  echo -e -n " \$(SRC_DIR)/ortools/${main_dir}/${name}.proto"
  all_deps=( $(get_dependencies "${file}") )
  if [[ "${#all_deps[@]}" != 0 ]]; then
    echo -e " \\"
    print_paths "${all_deps[@]/.proto/.pb.cc}"
  fi
  echo -e " | \$(GEN_DIR)/ortools/${main_dir}"
  echo -e "\t\$(PROTOC) --experimental_allow_proto3_optional --proto_path=\$(INC_DIR) \$(PROTOBUF_PROTOC_INC) --cpp_out=\$(GEN_PATH) \$(SRC_DIR)/ortools/${main_dir}/${name}.proto"
  echo
  # Print makefile command for .pb.h.
  echo -e "\$(GEN_DIR)/ortools/${main_dir}/${name}.pb.h: \\"
  echo -e " \$(GEN_DIR)/ortools/${main_dir}/${name}.pb.cc"
  echo -e "\t\$(TOUCH) \$(GEN_PATH)\$Sortools\$S${main_dir}\$S${name}.pb.h"
  echo
  # Print makefile command for .pb.$O.
  echo -e "\$(OBJ_DIR)/${main_dir}/${name}.pb.\$O: \\"
  echo -e -n " \$(GEN_DIR)/ortools/${main_dir}/${name}.pb.cc"
  echo -e " | \$(OBJ_DIR)/${main_dir}"
  echo -e "\t\$(CCC) \$(CFLAGS) -c \$(GEN_PATH)\$Sortools\$S${main_dir}\$S${name}.pb.cc \$(OBJ_OUT)\$(OBJ_DIR)\$S${main_dir}\$S${name}.pb.\$O"
  echo
done
