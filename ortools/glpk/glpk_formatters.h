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

// Formatting functions for GLPK constants.
#ifndef OR_TOOLS_GLPK_GLPK_FORMATTERS_H_
#define OR_TOOLS_GLPK_GLPK_FORMATTERS_H_

#include <string>
#include <string_view>

namespace operations_research {

// Formats a solution status (GLP_OPT,...).
std::string SolutionStatusString(int status);

// Formats a linear constraint or variable basis status (GLP_BS,...).
std::string BasisStatusString(int stat);

// Formats the return code of glp_intopt(), glp_simplex(), glp_exact() and
// glp_interior() to a string.
std::string ReturnCodeString(int rc);

// The maximum length of GLPK's names for the problem, the variables and the
// constraints.
inline constexpr std::size_t kMaxGLPKNameLen = 255;

// Returns a name which size is < kMaxGLPKNameLen and that does not contain
// control characters (as define by iscntrl()) (escaped using \xHH sequences).
std::string TruncateAndQuoteGLPKName(std::string_view original_name);

}  // namespace operations_research

#endif  // OR_TOOLS_GLPK_GLPK_FORMATTERS_H_
