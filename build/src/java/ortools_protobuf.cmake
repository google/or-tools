#TODO(Define find_program with proper error message depending on the program) 
find_program (PROTOC_EXECUTABLE protoc)

add_custom_command(
  OUTPUT  ${GEN_DIR}/com/google/ortools/constraintsolver/SearchLimitProtobuf.java
  COMMAND ${PROTOC_EXECUTABLE} --proto_path=${SRC_DIR} --java_out=${GEN_DIR} ${SRC_DIR}/constraint_solver/search_limit.proto
  DEPENDS ${SRC_DIR}/constraint_solver/search_limit.proto
  COMMENT "Protobuf : generating ${GEN_DIR}/com/google/ortools/constraintsolver/SearchLimitProtobuf.java"
)

add_custom_command(
  OUTPUT  ${GEN_DIR}/com/google/ortools/constraintsolver/SolverParameters.java
  COMMAND ${PROTOC_EXECUTABLE} --proto_path=${SRC_DIR} --java_out=${GEN_DIR} ${SRC_DIR}/constraint_solver/solver_parameters.proto
  DEPENDS ${SRC_DIR}/constraint_solver/solver_parameters.proto
  COMMENT "Protobuf : generating ${GEN_DIR}/com/google/ortools/constraintsolver/SolverParameters.java"
)
add_custom_command(
  OUTPUT  ${GEN_DIR}/com/google/ortools/constraintsolver/RoutingParameters.java
  COMMAND ${PROTOC_EXECUTABLE} --proto_path=${SRC_DIR} --java_out=${GEN_DIR} ${SRC_DIR}/constraint_solver/routing_parameters.proto
  DEPENDS ${SRC_DIR}/constraint_solver/routing_parameters.proto
  COMMENT "Protobuf : generating ${GEN_DIR}/com/google/ortools/constraintsolver/RoutingParameters.java"
)
add_custom_command(
  OUTPUT  ${GEN_DIR}/com/google/ortools/constraintsolver/RoutingEnums.java
  COMMAND ${PROTOC_EXECUTABLE} --proto_path=${SRC_DIR} --java_out=${GEN_DIR} ${SRC_DIR}/constraint_solver/routing_enums.proto
  DEPENDS ${SRC_DIR}/constraint_solver/routing_enums.proto
  COMMENT "Protobuf : generating ${GEN_DIR}/com/google/ortools/constraintsolver/RoutingEnums.java"
)

PREFIX_SUFFIX(
  PREFIX ${GEN_DIR}/com/google/ortools/constraintsolver/
  SUFFIX .java
  LIST SearchLimitProtobuf SolverParameters RoutingParameters RoutingEnums
  RESULT java_gen_protobuf
)


