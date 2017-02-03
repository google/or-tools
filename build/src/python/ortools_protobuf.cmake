#TODO(Define find_program with proper error message depending on the program) 
find_program (PROTOC_EXECUTABLE protoc)

#TODO(make the python version configurable)
find_package( PythonInterp 2.7 REQUIRED )

list(APPEND python_proto_sources constraint_solver/search_limit constraint_solver/model constraint_solver/assignment constraint_solver/solver_parameters constraint_solver/routing_enums linear_solver/linear_solver)


set(PY_GEN_DIR ${GEN_DIR}/ortools)
file(MAKE_DIRECTORY ${PY_GEN_DIR})

set(python_gen_protobuf "")

foreach(fil ${python_proto_sources})
  set(out_file ${PY_GEN_DIR}/${fil}_pb2.py)
  set(proto_file ${SRC_DIR}/${fil}.proto)
  add_custom_command(
    OUTPUT  ${out_file}
    COMMAND ${PROTOC_EXECUTABLE} --proto_path=${SRC_DIR} --python_out=${PY_GEN_DIR} ${proto_file}
    DEPENDS ${proto_file}
    COMMENT "Protobuf : generating ${out_file}"
  )
  list(APPEND python_gen_protobuf ${out_file})
endforeach()

set(out_file ${PY_GEN_DIR}/constraint_solver/routing_parameters_pb2.py)
set(proto_file ${SRC_DIR}/constraint_solver/routing_parameters.proto)
add_custom_command(
  OUTPUT  ${out_file}
  COMMAND ${PROTOC_EXECUTABLE} --proto_path=${SRC_DIR} --python_out=${PY_GEN_DIR} ${proto_file}
  DEPENDS ${proto_file} ${SRC_DIR}/constraint_solver/solver_parameters.proto
  COMMENT "Protobuf : generating ${out_file}"
)
list(APPEND python_gen_protobuf ${out_file})

add_custom_command(
  OUTPUT  ${PROTO_PYTHON_DIR}/google/protobuf/descriptor_pb2.py
  COMMAND cd ${PROTO_PYTHON_DIR} && ${PYTHON_EXECUTABLE} setup.py build
  DEPENDS ${PROTO_PYTHON_DIR}/setup.py
  COMMENT "Protobuf : ${PROTO_PYTHON_DIR}/google/protobuf/descriptor_pb2.py"
)
list(APPEND python_gen_protobuf ${PROTO_PYTHON_DIR}/google/protobuf/descriptor_pb2.py)




