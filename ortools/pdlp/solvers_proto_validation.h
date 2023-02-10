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

// Validation utilities for solvers.proto.

#ifndef PDLP_SOLVERS_PROTO_VALIDATION_H_
#define PDLP_SOLVERS_PROTO_VALIDATION_H_

#include "absl/status/status.h"
#include "ortools/pdlp/solvers.pb.h"

namespace operations_research::pdlp {

// Returns `InvalidArgumentError` if the proto contains invalid values. Returns
// `OkStatus` otherwise.
absl::Status ValidateTerminationCriteria(const TerminationCriteria& criteria);

// Returns `InvalidArgumentError` if the proto contains invalid values. Returns
// `OkStatus` otherwise.
absl::Status ValidateAdaptiveLinesearchParams(
    const AdaptiveLinesearchParams& params);

// Returns `InvalidArgumentError` if the proto contains invalid values. Returns
// `OkStatus` otherwise.
absl::Status ValidateMalitskyPockParams(const MalitskyPockParams& params);

// Returns `InvalidArgumentError` if the proto contains invalid values. Returns
// `OkStatus` otherwise.
absl::Status ValidatePrimalDualHybridGradientParams(
    const PrimalDualHybridGradientParams& params);

}  // namespace operations_research::pdlp

#endif  // PDLP_SOLVERS_PROTO_VALIDATION_H_
