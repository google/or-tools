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

#ifndef OR_TOOLS_SAT_CP_MODEL_SEARCH_H_
#define OR_TOOLS_SAT_CP_MODEL_SEARCH_H_

#include <functional>
#include <vector>

#include "ortools/base/integral_types.h"
#include "ortools/sat/cp_model.pb.h"
#include "ortools/sat/integer.h"
#include "ortools/sat/integer_search.h"
#include "ortools/sat/model.h"

namespace operations_research {
namespace sat {

// Constructs the search strategy specified in the given CpModelProto. A
// positive variable ref in the proto is mapped to variable_mapping[ref] in the
// model. All the variables referred in the search strategy must be correctly
// mapped, the other entries can be set to kNoIntegerVariable.
std::function<BooleanOrIntegerLiteral()> ConstructSearchStrategy(
    const CpModelProto& cp_model_proto,
    const std::vector<IntegerVariable>& variable_mapping,
    IntegerVariable objective_var, Model* model);

// For debugging fixed-search: display information about the named variables
// domain before taking each decision. Note that we copy the instrumented
// stategy so it doesn't have to outlive the returned functions like the other
// arguments.
std::function<BooleanOrIntegerLiteral()> InstrumentSearchStrategy(
    const CpModelProto& cp_model_proto,
    const std::vector<IntegerVariable>& variable_mapping,
    const std::function<BooleanOrIntegerLiteral()>& instrumented_strategy,
    Model* model);

// Returns up to "num_workers" different parameters. We do not always return
// num_worker parameters to leave room for strategies like LNS that do not
// consume a full worker and can always be interleaved.
std::vector<SatParameters> GetDiverseSetOfParameters(
    const SatParameters& base_params, const CpModelProto& cp_model,
    const int num_workers);

}  // namespace sat
}  // namespace operations_research

#endif  // OR_TOOLS_SAT_CP_MODEL_SEARCH_H_
