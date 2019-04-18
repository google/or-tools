// Copyright 2010-2018 Google LLC
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

#include "ortools/lp_data/model_reader.h"

#include "ortools/base/file.h"
#include "ortools/linear_solver/linear_solver.pb.h"
#include "ortools/lp_data/proto_utils.h"
#include "ortools/util/file_util.h"

namespace operations_research {
namespace glop {

bool LoadLinearProgramFromModelOrRequest(const std::string& input_file_path,
                                         LinearProgram* linear_program) {
  MPModelProto model_proto;
  MPModelRequest request_proto;
  ReadFileToProto(input_file_path, &model_proto);
  ReadFileToProto(input_file_path, &request_proto);
  // If the input proto is in binary format, both ReadFileToProto could return
  // true. Instead use the actual number of variables found to test the
  // correct format of the input.
  const bool is_model_proto = model_proto.variable_size() > 0;
  const bool is_request_proto = request_proto.model().variable_size() > 0;
  if (!is_model_proto && !is_request_proto) {
    LOG(ERROR) << "Failed to parse '" << input_file_path
               << "' as an MPModelProto or an MPModelRequest.";
    return false;
  } else {
    if (is_model_proto && is_request_proto) {
      LOG(ERROR) << input_file_path
                 << " is parsing as both MPModelProto and MPModelRequest";
      return false;
    }
    if (is_request_proto) {
      VLOG(1) << "Read input proto as an MPModelRequest.";
      model_proto.Swap(request_proto.mutable_model());
    } else {
      VLOG(1) << "Read input proto as an MPModelProto.";
    }
  }
  MPModelProtoToLinearProgram(model_proto, linear_program);
  return true;
}

}  // namespace glop
}  // namespace operations_research
