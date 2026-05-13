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

// Parses a the model in "CPLEX LP" format using SCIP.
//
// Note that ../../lp_data/lp_parser.h parses ".lp" files in the LPSolve
// version of the LP format, which is different from the (now) more standard
// CPLEX version of the LP format. These formats are not compatible. See
//  https://lpsolve.sourceforge.net/5.5/lp-format.htm
//  https://lpsolve.sourceforge.net/5.5/CPLEX-format.htm
// for a comparison.
#ifndef OR_TOOLS_MATH_OPT_IO_LP_PARSER_H_
#define OR_TOOLS_MATH_OPT_IO_LP_PARSER_H_

#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"
#include "ortools/math_opt/model.pb.h"

namespace operations_research::math_opt {

// Parses a the model in "CPLEX LP" format.
//
// This function creates and destroys local temporary files and thus is not
// portable.
//
// For large models, this will not work on diskless jobs in prod.
//
// Warnings:
//  * Only a linear objective and linear constraints are supported. When SCIP is
//    used, indicator constraints are also supported.
//  * The names of indicator constraints are not preserved when using SCIP
//  * The variables may be permuted.
//  * Two sided constraints are not in the LP format. If you round trip a Model
//    Proto with lp_converter.h, the two sided constraints are and rewritten as
//    two one sided constraints with new names.
//
// OR-Tools does not have an LP file parser, so we go from LP file to SCIP, then
// export to MPS, parse the MPS to ModelProto. This is not efficient, but
// usually still much faster than solving an LP or MIP. Note the SCIP LP parser
// actually supports SOS and quadratics, but the OR-tools MPS reader does not.
//
// It would be preferable to write an LP Parser from scratch and delete this.
//
// For more information about the different LP file formats:
// http://lpsolve.sourceforge.net/5.5/lp-format.htm
// http://lpsolve.sourceforge.net/5.5/CPLEX-format.htm
// https://www.ibm.com/docs/en/icos/12.8.0.0?topic=cplex-lp-file-format-algebraic-representation
// http://www.gurobi.com/documentation/5.1/reference-manual/node871
absl::StatusOr<ModelProto> ModelProtoFromLp(absl::string_view lp_data);

}  // namespace operations_research::math_opt

#endif  // OR_TOOLS_MATH_OPT_IO_LP_PARSER_H_
