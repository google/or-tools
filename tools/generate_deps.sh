#!/bin/bash
declare -r libname="${1}"
declare -r main_dir="${2}"

# List all files on ortools/"${main_dir}"
all_cc=( $(ls ortools/"${main_dir}"/*.cc | grep -v test.cc) )
all_h=( $(ls ortools/"${main_dir}"/*.h) )
declare -a all_proto
if ls ortools/"${main_dir}"/*proto >& /dev/null; then
    all_proto+=( $(ls ortools/"${main_dir}"/*.proto) )
fi

# Arguments: a list of dependencies.
# Output: one line per dependency, listing its actual path in ortools,
#         as understood by the Makefile (eg. using $(SRC_DIR) etc).
#         Each line is appended with an ending '\', except the last one.
#
# Many kinds of input dependencies are supported:
# - Standard files, like like "ortools/base/stringprintf.h": those will
#   be output with the $(SRC_DIR) directory prefix.
# - Generated proto file *.pb.{h,cc}: those will be output with the
#   $(GEN_DIR) prefix
# - Object files *.$O: those will be output with the $(OBJ_DIR) prefix.
function print_paths {
  is_first_line=1
  for dep in "$@"; do
    ((is_first_line)) || echo " \\"
    is_first_line=0
    local dir="SRC_DIR"
    [[ "${dep}" == *.pb.* ]] && dir="GEN_DIR"
    if [[ "${dep}" == *.\$O ]]; then
      dir="OBJ_DIR"
      dep="${dep/ortools\/}"  # Remove the "ortools/" directory.
    fi
    echo -n "    \$(${dir})/${dep}"
  done
  echo ""
}

# Input: a list of file names (eg. "ortools/base/file.h").
# Output: all the files these files depend on (given by their #include,
#         by their "import" for proto files).
function get_dependencies {
   grep -e "^\(#include\|import\) \"ortools/" "$*"\
     | cut -d '"' -f 2 | sort -u
}

# Generate XXX_DEPS macro
echo "${libname}"_DEPS = \\
print_paths ortools/"${main_dir}"/*.h
echo

# generate XXX_LIB_OBJS macro.
echo "${1}_LIB_OBJS = \\"
print_paths "${all_cc[@]//\.cc/\.\$O}" "${all_proto[@]//\.proto/.pb.\$O}"
echo

# Generate dependencies for .h files
for file in "${all_h[@]}"; do
  name=$(basename "${file}" .h)
  # Compute dependencies.
  all_deps=( $(get_dependencies "${file}") )
  # Print makefile command.
  if [[ "${#all_deps[@]}" != 0 ]]
  then
    echo "\$(SRC_DIR)/ortools/${main_dir}/${name}.h: \\"
    print_paths "${all_deps[@]}"
    echo
  fi
done

# Generate dependencies and compilation command for .cc files.
for file in "${all_cc[@]}"
do
  name=$(basename "${file}" .cc)
  echo "\$(OBJ_DIR)/${main_dir}/${name}.\$O: \\"
  echo "    \$(SRC_DIR)/ortools/${main_dir}/${name}.cc \\"
  all_deps=( $(get_dependencies "${file}") )
  print_paths "${all_deps[@]}"
  echo -e "\t\$(CCC) \$(CFLAGS) -c \$(SRC_DIR)\$Sortools\$S${main_dir}\$S${name}.cc \$(OBJ_OUT)\$(OBJ_DIR)\$S${main_dir}\$S${name}.\$O"
  echo
done

# Generate dependencies, compulation, and protoc command for .proto files.
for file in "${all_proto[@]}"
do
  name=$(basename "${file}" .proto)
  echo "\$(GEN_DIR)/ortools/${main_dir}/${name}.pb.cc: \\"
  echo -n "    \$(SRC_DIR)/ortools/${main_dir}/${name}.proto"
  all_deps=( $(get_dependencies "${file}") )
  if [[ "${#all_deps[@]}" != 0 ]]; then
    echo " \\"
    print_paths "${all_deps[@]//\.proto/.pb.cc}"
  else
    echo
  fi
  echo -e "\t\$(PROTOC) --proto_path=\$(INC_DIR) \$(PROTOBUF_PROTOC_INC) --cpp_out=\$(GEN_DIR) \$(SRC_DIR)/ortools/${main_dir}/${name}.proto"
  echo
  echo "\$(GEN_DIR)/ortools/${main_dir}/${name}.pb.h: \$(GEN_DIR)/ortools/${main_dir}/${name}.pb.cc \\"
  echo
  echo "\$(OBJ_DIR)/${main_dir}/${name}.pb.\$O: \$(GEN_DIR)/ortools/${main_dir}/${name}.pb.cc"
  echo -e "\t\$(CCC) \$(CFLAGS) -c \$(GEN_DIR)/ortools/${main_dir}/${name}.pb.cc \$(OBJ_OUT)\$(OBJ_DIR)\$S${main_dir}\$S${name}.pb.\$O"
  echo
done
