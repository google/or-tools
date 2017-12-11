#!/bin/bash
main_dir=$2

# List all files on ortools/"${main_dir}"
all_cc=( $(ls ortools/"${main_dir}"/*.cc | grep -v test.cc) )
all_h=( $(ls ortools/"${main_dir}"/*.h) )
declare -a all_proto
if ls ortools/"${main_dir}"/*proto >& /dev/null; then
    all_proto+=( $(ls ortools/"${main_dir}"/*.proto) )
fi

function print_include_list {
  # We want to print a ' \' at the end of every line but the last.
  is_first_line=1
  for dep in "$@"; do
    ((is_first_line)) || echo " \\"
    is_first_line=0
    local dir="SRC_DIR"
    [[ "${dep}" == *.pb.* ]] && dir="GEN_DIR"
    echo -n "    \$(${dir})/${dep}"
  done
  echo ""
}

# Args: $1 = the prefix
#       $2 and all following = lines to print preceded by the prefix
# All lines but the last will finish with a backslash  '\'.
function print_backslashed_lines_with_prefix {
  local -r prefix="$1"
  shift
  local -r lines=( "$@" )
  local -r numlines="$#"
  for line in "${lines[@]::numlines-1}"; do
    echo "${prefix}${line} \\"
  done
  echo "${prefix}${lines[numlines-1]}"
}

# Generate XXX_DEPS macro
declare -a all_deps
for dir in "${@:2}"; do
  for deps in $(grep -e "^\#include \"ortools/${dir}" ortools/"${dir}"/*.h | cut -d '"' -f 2 | sort -u); do
    all_deps+=( "${deps}" )
  done
done
echo "$1"_DEPS = \\
print_include_list "${all_deps[@]}"
echo

# generate XXX_LIB_OBJS macro.
# Get list of obj files to build.
all_cc_objs_tmp=( "${all_cc[@]//ortools/\$(OBJ_DIR)}" )
all_cc_objs=( "${all_cc_objs_tmp[@]//\.cc/\.\$O}" )
all_proto_objs_tmp=( "${all_proto[@]//ortools/\$(OBJ_DIR)}" )
all_proto_objs=( "${all_proto_objs_tmp[@]//\.proto/.pb.\$O}" )
all_objs+=( "${all_cc_objs[@]}" "${all_proto_objs[@]}" )

# Print makefile macro definition.
echo "${1}_LIB_OBJS = \\"
print_backslashed_lines_with_prefix "    " "${all_objs[@]}"
echo

# Generate dependencies for .h files
for file in "${all_h[@]}"; do
  name=$(basename "${file}" .h)
  # Compute dependencies.
  all_deps=( )
  for dir in "${@:2}"; do
    all_deps+=( $(grep -e "^\#include \"ortools/${dir}" "${file}" | cut -d '"' -f 2 | sort -u) )
  done
  # Print makefile command.
  if [[ "${#all_deps[@]}" != 0 ]]
  then
    echo "\$(SRC_DIR)/ortools/${2}/${name}.h: \\"
    print_include_list "${all_deps[@]}"
    echo
  fi
done

# Generate dependencies and compilation command for .cc files.
for file in "${all_cc[@]}"
do
  name=$(basename "${file}" .cc)
  # Compute dependencies.
  all_deps=( )
  for dir in "${@:2}"; do
    all_deps+=( $(grep -e "^\#include \"ortools/${dir}" "${file}" | cut -d '"' -f 2 | sort -u) )
  done
  # Print makefile command.
  echo "\$(OBJ_DIR)/$2/${name}.\$O: \\"
  echo "    \$(SRC_DIR)/ortools/$2/${name}.cc \\"
  print_include_list "${all_deps[@]}"
  echo -e "\t\$(CCC) \$(CFLAGS) -c \$(SRC_DIR)\$Sortools\$S${2}\$S${name}.cc \$(OBJ_OUT)\$(OBJ_DIR)\$S${2}\$S${name}.\$O"
  echo
done

# Generate dependencies, compulation, and protoc command for .proto files.
for file in "${all_proto[@]}"
do
  name=$(basename "${file}" .proto)
  # Compute inter proto dependencies.
  all_deps=( )
  for dir in "${@:2}"; do
    all_deps+=( $(grep -e "^import \"ortools/${dir}" "${file}" | cut -d '"' -f 2 | sed -e "s/proto/pb.h/" | sort -u) )
  done
  # Print makefile command.
  echo "\$(GEN_DIR)/ortools/$2/${name}.pb.cc: \$(SRC_DIR)/ortools/$2/${name}.proto"
  echo -e "\t\$(PROTOBUF_DIR)/bin/protoc --proto_path=\$(INC_DIR) --cpp_out=\$(GEN_DIR) \$(SRC_DIR)/ortools/${2}/${name}.proto"
  echo
  if [[ "${#all_deps[@]}" != 0 ]]; then
    echo "\$(GEN_DIR)/ortools/${2}/${name}.pb.h: \$(GEN_DIR)/ortools/${2}/${name}.pb.cc \\"
    print_include_list "${all_deps[@]}"
  else
    echo "\$(GEN_DIR)/ortools/${2}/${name}.pb.h: \$(GEN_DIR)/ortools/${2}/${name}.pb.cc"
  fi
  echo
  echo "\$(OBJ_DIR)/${2}/${name}.pb.\$O: \$(GEN_DIR)/ortools/${2}/${name}.pb.cc"
  echo -e "\t\$(CCC) \$(CFLAGS) -c \$(GEN_DIR)/ortools/${2}/${name}.pb.cc \$(OBJ_OUT)\$(OBJ_DIR)\$S${2}\$S${name}.pb.\$O"
  echo
done
