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

#ifndef OR_TOOLS_LP_DATA_MODEL_READER_H_
#define OR_TOOLS_LP_DATA_MODEL_READER_H_

#include "ortools/linear_solver/linear_solver.pb.h"
#include "ortools/lp_data/lp_data.h"

namespace operations_research {
namespace glop {

// Helper function to read data from mps files into LinearProgram.
bool LoadLinearProgramFromMps(const std::string& input_file_path,
                              const std::string& forced_mps_format,
                              LinearProgram* linear_program);

// Helper function to read data from model files into LinearProgram.
bool LoadLinearProgramFromModelOrRequest(const std::string& input_file_path,
                                         LinearProgram* linear_program);

}  // namespace glop
}  // namespace operations_research

#endif  // OR_TOOLS_LP_DATA_MODEL_READER_H_
