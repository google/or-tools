#This is a modified copy of https://github.com/Kitware/CMake/blob/master/Modules/FindProtobuf.cmake.
#We edited it because we need to control where the files are generated.

#TODO(check how to install protobuf : On Ubuntu sudo apt-get install protobuf_complier libprotobuf-dev)

function(OR_TOOLS_PROTOBUF_GENERATE_CPP SRCS HDRS)
  if(NOT ARGN)
    message(SEND_ERROR "Error: PROTOBUF_GENERATE_CPP() called without any proto files")
    return()
  endif()
  find_program (PROTOC_EXECUTABLE protoc)
  set(${SRCS} "")
  set(${HDRS} "")
  foreach(FIL ${ARGN})
    get_filename_component(FIL_WE ${FIL} NAME_WE)
    get_filename_component(FIL_REL_DIR ${FIL} DIRECTORY)
    set(out_pb_cc "${GEN_DIR}/${FIL_REL_DIR}/${FIL_WE}.pb.cc")
    set(out_pb_h "${GEN_DIR}/${FIL_REL_DIR}/${FIL_WE}.pb.h")
    list(APPEND ${SRCS} "${out_pb_cc}")
    list(APPEND ${HDRS} "${out_pb_h}")
    set(COMMAND ${PROTOC_EXECUTABLE} --proto_path ${SRC_DIR} --cpp_out=${GEN_DIR} ${SRC_DIR}/${FIL})
    add_custom_command(
      OUTPUT  ${out_pb_cc} ${out_pb_h}
      COMMAND ${COMMAND}
      DEPENDS ${SRC_DIR}/${FIL}
      COMMENT "Running protocol buffer compiler : ${COMMAND}"
    )
  endforeach()
  set(${SRCS} ${${SRCS}} PARENT_SCOPE)
  set(${HDRS} ${${HDRS}} PARENT_SCOPE)
endfunction()

