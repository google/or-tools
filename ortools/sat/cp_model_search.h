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

#ifndef OR_TOOLS_SAT_CP_MODEL_SEARCH_H_
#define OR_TOOLS_SAT_CP_MODEL_SEARCH_H_

#include <cstdint>
#include <functional>
#include <vector>

#include "ortools/base/integral_types.h"
#include "ortools/sat/cp_model.pb.h"
#include "ortools/sat/cp_model_mapping.h"
#include "ortools/sat/integer.h"
#include "ortools/sat/integer_search.h"
#include "ortools/sat/model.h"
#include "ortools/sat/sat_base.h"
#include "ortools/sat/sat_parameters.pb.h"

namespace operations_research {
namespace sat {

// This class allows to query information about the current bounds of the loaded
// cp_model.proto variables during the search. It is a "view" of the current
// solver state using the indices of the proto.
//
// TODO(user): For now it uses proto indices of the loaded model. We will need
// to add a mapping to use proto indices of the non-presolved model to allow for
// a client custom search with presolve. The main API shouldn't change though
// and the change will be transparent.
class CpModelView {
 public:
  explicit CpModelView(Model* model);

  // The valid indices for the calls below are in [0, num_variables).
  int NumVariables() const;

  // Getters about the current domain of the given variable.
  bool IsFixed(int var) const;
  int64_t Min(int var) const;
  int64_t Max(int var) const;

  // If under a given partial assignment, the value of a variable has no impact,
  // this might returns true, and there is no point trying to branch on this
  // variable.
  //
  // This might for example be the case for the start of an unperformed interval
  // which will not impact the rest of the problem in any way. Note that it is
  // still possible to branch on ignored variable, this will just not change
  // anything.
  bool IsCurrentlyFree(int var) const;

  // Helpers to generate a decision.
  BooleanOrIntegerLiteral GreaterOrEqual(int var, int64_t value) const;
  BooleanOrIntegerLiteral LowerOrEqual(int var, int64_t value) const;
  BooleanOrIntegerLiteral MedianValue(int var) const;

 private:
  const CpModelMapping& mapping_;
  const VariablesAssignment& boolean_assignment_;
  const IntegerTrail& integer_trail_;
  const IntegerEncoder& integer_encoder_;
};

// Constructs the search strategy specified in the given CpModelProto.
std::function<BooleanOrIntegerLiteral()> ConstructUserSearchStrategy(
    const CpModelProto& cp_model_proto, Model* model);

// Constructs our "fixed" search strategy which start with
// ConstructUserSearchStrategy() but is completed by a couple of automatic
// heuristics.
std::function<BooleanOrIntegerLiteral()> ConstructFixedSearchStrategy(
    const CpModelProto& cp_model_proto,
    const std::vector<IntegerVariable>& variable_mapping,
    IntegerVariable objective_var, Model* model);

// For debugging fixed-search: display information about the named variables
// domain before taking each decision. Note that we copy the instrumented
// strategy so it doesn't have to outlive the returned functions like the other
// arguments.
std::function<BooleanOrIntegerLiteral()> InstrumentSearchStrategy(
    const CpModelProto& cp_model_proto,
    const std::vector<IntegerVariable>& variable_mapping,
    const std::function<BooleanOrIntegerLiteral()>& instrumented_strategy,
    Model* model);

// Returns up to base_params.num_workers() different parameters.
// We do not always return num_worker parameters to leave room for strategies
// like LNS that do not consume a full worker and can always be interleaved.
std::vector<SatParameters> GetDiverseSetOfParameters(
    const SatParameters& base_params, const CpModelProto& cp_model);

// Returns a vector of num_params_to_generate set of parameters to specify
// solvers specialized to find a initial solution.
std::vector<SatParameters> GetFirstSolutionParams(
    const SatParameters& base_params, const CpModelProto& cp_model,
    int num_params_to_generate);

}  // namespace sat
}  // namespace operations_research

#endif  // OR_TOOLS_SAT_CP_MODEL_SEARCH_H_
