// Copyright 2010-2025 Google LLC
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef OR_TOOLS_LINEAR_SOLVER_PROTO_SOLVER_PROTO_UTILS_H_
#define OR_TOOLS_LINEAR_SOLVER_PROTO_SOLVER_PROTO_UTILS_H_

#include <string>
#include <type_traits>
#include <utility>

#include "absl/log/check.h"
#include "absl/log/log.h"
#include "absl/status/statusor.h"
#include "google/protobuf/message.h"
#include "ortools/linear_solver/linear_solver.pb.h"
#include "ortools/port/proto_utils.h"

namespace operations_research {

using google::protobuf::Message;

// Some SolveWithProto() returns a StatusOr<MPModelResponse>, this utility
// just convert bad absl::StatusOr to a proper error in MPModelResponse.
//
// TODO(user): All SolveWithProto() should just fill the appropriate response
// instead.
inline MPSolutionResponse ConvertStatusOrMPSolutionResponse(
    bool log_error, absl::StatusOr<MPSolutionResponse> response) {
  if (!response.ok()) {
    if (log_error) {
      LOG(ERROR) << "Error status: " << response.status();
    }
    MPSolutionResponse error_response;
    error_response.set_status(MPSolverResponseStatus::MPSOLVER_ABNORMAL);
    error_response.set_status_str(response.status().ToString());
    return error_response;
  }
  return std::move(response).value();
}

// Returns a string that should be used in MPModelRequest's
// solver_specific_parameters field to encode the glop parameters.
//
// The returned string's content depends on the version of the proto library
// that is linked in the binary.
//
// By default it will contain the textual representation of the input proto.
// But when the proto-lite is used, it will contain the binary stream of the
// proto instead since it is not possible to build the textual representation in
// that case.
//
// This function will test if the proto-lite is used and expect a binary stream
// when it is the case. So in order for your code to be portable, you should
// always use this function to set the specific parameters.
//
// Proto-lite disables some features of protobufs and messages inherit from
// MessageLite directly instead of inheriting from Message (which is itself a
// specialization of MessageLite).
// See https://protobuf.dev/reference/cpp/cpp-generated/#message for details.
template <typename P>
std::string EncodeParametersAsString(const P& parameters) {
  if constexpr (!std::is_base_of<Message, P>::value) {
    // Here we use SerializeToString() instead of SerializeAsString() since the
    // later ignores errors and returns an empty string instead (which can be a
    // valid value when no fields are set).
    std::string bytes;
    CHECK(parameters.SerializeToString(&bytes));
    return bytes;
  }

  return ProtobufShortDebugString(parameters);
}

}  // namespace operations_research

#endif  // OR_TOOLS_LINEAR_SOLVER_PROTO_SOLVER_PROTO_UTILS_H_
