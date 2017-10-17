// Copyright 2010-2017 Google
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
#include "ortools/lp_data/mps_reader.h"
#include "ortools/lp_data/proto_utils.h"
#include "ortools/util/file_util.h"

namespace operations_research {
namespace glop {

bool LoadLinearProgramFromMps(const std::string& input_file_path,
                              const std::string& forced_mps_format,
                              LinearProgram* linear_program) {
  LinearProgram linear_program_fixed;
  LinearProgram linear_program_free;
  MPSReader mps_reader;
  mps_reader.set_log_errors(forced_mps_format == "free" ||
                            forced_mps_format == "fixed");
  bool fixed_read = forced_mps_format != "free" &&
                    mps_reader.LoadFileWithMode(input_file_path, false,
                                                &linear_program_fixed);
  const bool free_read =
      forced_mps_format != "fixed" &&
      mps_reader.LoadFileWithMode(input_file_path, true, &linear_program_free);
  if (!fixed_read && !free_read) {
    LOG(ERROR) << "Error while parsing the mps file '" << input_file_path
               << "' Use the --forced_mps_format flags to see the errors.";
    return false;
  }
  if (fixed_read && free_read) {
    if (linear_program_fixed.name() != linear_program_free.name()) {
      VLOG(1) << "Name of the model differs between fixed and free forms. "
              << "Fallbacking to free form.";
      fixed_read = false;
    }
  }
  if (!fixed_read) {
    VLOG(1) << "Read file in free format.";
    linear_program->PopulateFromLinearProgram(linear_program_free);
  } else {
    VLOG(1) << "Read file in fixed format.";
    linear_program->PopulateFromLinearProgram(linear_program_fixed);
    if (free_read) {
      // TODO(user): Dump() take ages on large program, so we need an efficient
      // comparison function between two linear programs. Using
      // GetProblemStats() for now.
      if (linear_program_free.GetProblemStats() !=
          linear_program_fixed.GetProblemStats()) {
        LOG(ERROR) << "Could not decide if '" << input_file_path
                   << "' is in fixed or free format.";
        return false;
      }
    }
  }
  return true;
}

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
