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

#ifndef OR_TOOLS_LINEAR_SOLVER_SCIP_HELPER_MACROS_H_
#define OR_TOOLS_LINEAR_SOLVER_SCIP_HELPER_MACROS_H_

#include "absl/status/status.h"
#include "absl/strings/str_format.h"
#include "ortools/base/status_macros.h"

namespace operations_research {
namespace internal {
// Our own version of SCIP_CALL to do error management.
// NOTE(user): There are so many SCIP error codes, in so many different
// situations. We don't try to match them perfectly to google3 error codes.
// Instead, we use the most likely/generic code "invalid argument" and surface
// the internal SCIP error code to the user.
inline absl::Status ScipCodeToUtilStatus(/*SCIP_Retcode*/ int retcode,
                                         const char* source_file,
                                         int source_line,
                                         const char* scip_statement) {
  if (retcode == /*SCIP_OKAY*/ 1) return absl::OkStatus();
  return absl::InvalidArgumentError(
      absl::StrFormat("SCIP error code %d (file '%s', line %d) on '%s'",
                      retcode, source_file, source_line, scip_statement));
}
}  // namespace internal

#define SCIP_TO_STATUS(x)                                                      \
  ::operations_research::internal::ScipCodeToUtilStatus(x, __FILE__, __LINE__, \
                                                        #x)

#define RETURN_IF_SCIP_ERROR(x) RETURN_IF_ERROR(SCIP_TO_STATUS(x));

}  // namespace operations_research

#endif  // OR_TOOLS_LINEAR_SOLVER_SCIP_HELPER_MACROS_H_
