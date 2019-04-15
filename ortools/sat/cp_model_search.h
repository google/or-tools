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
std::function<LiteralIndex()> ConstructSearchStrategy(
    const CpModelProto& cp_model_proto,
    const std::vector<IntegerVariable>& variable_mapping,
    IntegerVariable objective_var, Model* model);

// For debugging fixed-search: display information about the named variables
// domain before taking each decision. Note that we copy the instrumented
// stategy so it doesn't have to outlive the returned functions like the other
// arguments.
std::function<LiteralIndex()> InstrumentSearchStrategy(
    const CpModelProto& cp_model_proto,
    const std::vector<IntegerVariable>& variable_mapping,
    const std::function<LiteralIndex()>& instrumented_strategy, Model* model);

// Returns a different parameters depending on the given worker_id.
// This assumes that worker will get an id in [0, num_workers).
//
// TODO(user): Find a way to know how many search heuristics there are,
// and how many threads to pass to LNS with an optimization model.
SatParameters DiversifySearchParameters(const SatParameters& params,
                                        const CpModelProto& cp_model,
                                        const int worker_id, std::string* name);

}  // namespace sat
}  // namespace operations_research

#endif  // OR_TOOLS_SAT_CP_MODEL_SEARCH_H_
