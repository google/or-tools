// Copyright 2010-2022 Google LLC
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

#ifndef OR_TOOLS_MATH_OPT_IO_LP_CONVERTER_H_
#define OR_TOOLS_MATH_OPT_IO_LP_CONVERTER_H_

#include <string>

#include "absl/status/statusor.h"
#include "ortools/math_opt/model.pb.h"

namespace operations_research::math_opt {

// Returns the model in "CPLEX LP" format.
//
// The RemoveNames() function can be used on the model to remove names if they
// should not be exported.
//
// For more information about the different LP file formats:
// http://lpsolve.sourceforge.net/5.5/lp-format.htm
// http://lpsolve.sourceforge.net/5.5/CPLEX-format.htm
// https://www.ibm.com/docs/en/icos/12.8.0.0?topic=cplex-lp-file-format-algebraic-representation
// http://www.gurobi.com/documentation/5.1/reference-manual/node871
absl::StatusOr<std::string> ModelProtoToLp(const ModelProto& model);

}  // namespace operations_research::math_opt

#endif  // OR_TOOLS_MATH_OPT_IO_LP_CONVERTER_H_
