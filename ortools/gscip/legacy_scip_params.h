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

#ifndef OR_TOOLS_GSCIP_LEGACY_SCIP_PARAMS_H_
#define OR_TOOLS_GSCIP_LEGACY_SCIP_PARAMS_H_

#include <string>

#include "absl/status/status.h"
#include "absl/strings/string_view.h"
#include "scip/scip.h"
#include "scip/type_scip.h"

namespace operations_research {

absl::Status LegacyScipSetSolverSpecificParameters(absl::string_view parameters,
                                                   SCIP* scip);
}

#endif  // OR_TOOLS_GSCIP_LEGACY_SCIP_PARAMS_H_
