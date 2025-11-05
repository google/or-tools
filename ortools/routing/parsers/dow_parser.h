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

#ifndef ORTOOLS_ROUTING_PARSERS_DOW_PARSER_H_
#define ORTOOLS_ROUTING_PARSERS_DOW_PARSER_H_

#include "absl/status/status.h"
#include "absl/strings/string_view.h"
#include "ortools/routing/parsers/capacity_planning.pb.h"

// Reader for Multicommodity fixed-charge Network Design (MCND) files using the
// .dow format.

namespace operations_research::routing {
::absl::Status ReadFile(absl::string_view file_name,
                        CapacityPlanningInstance* request);
}  // namespace operations_research::routing

#endif  // ORTOOLS_ROUTING_PARSERS_DOW_PARSER_H_
