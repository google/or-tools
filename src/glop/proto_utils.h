#ifndef OR_TOOLS_GLOP_PROTO_UTILS_H_
#define OR_TOOLS_GLOP_PROTO_UTILS_H_

#include "linear_solver/linear_solver2.pb.h"
#include "glop/lp_data.h"

namespace operations_research {
namespace glop {

// Converts a LinearProgram to a MPModelProto.
void LinearProgramToMPModelProto(const LinearProgram& input,
                                 new_proto::MPModelProto* output);

// Converts a MPModelProto to a LinearProgram.
void MPModelProtoToLinearProgram(const new_proto::MPModelProto& input,
                                 LinearProgram* output);

}  // namespace glop
}  // namespace operations_research

#endif  // OR_TOOLS_GLOP_PROTO_UTILS_H_
