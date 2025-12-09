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

#ifndef ORTOOLS_SAT_CP_MODEL_SEARCH_H_
#define ORTOOLS_SAT_CP_MODEL_SEARCH_H_

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "absl/random/bit_gen_ref.h"
#include "absl/strings/string_view.h"
#include "absl/types/span.h"
#include "ortools/base/stl_util.h"
#include "ortools/sat/cp_model.pb.h"
#include "ortools/sat/cp_model_mapping.h"
#include "ortools/sat/integer.h"
#include "ortools/sat/integer_base.h"
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

  // Helpers to generate a decision.
  BooleanOrIntegerLiteral GreaterOrEqual(int var, int64_t value) const;
  BooleanOrIntegerLiteral LowerOrEqual(int var, int64_t value) const;
  BooleanOrIntegerLiteral MedianValue(int var) const;
  BooleanOrIntegerLiteral RandomSplit(int var, int64_t lb, int64_t ub) const;

 private:
  const CpModelMapping& mapping_;
  const VariablesAssignment& boolean_assignment_;
  const IntegerTrail& integer_trail_;
  const IntegerEncoder& integer_encoder_;
  mutable absl::BitGenRef random_;
};

// Constructs the search strategy specified in the given CpModelProto.
std::function<BooleanOrIntegerLiteral()> ConstructUserSearchStrategy(
    const CpModelProto& cp_model_proto, Model* model);

// Constructs a search strategy tailored for the current model.
std::function<BooleanOrIntegerLiteral()> ConstructHeuristicSearchStrategy(
    const CpModelProto& cp_model_proto, Model* model);

// Constructs an integer completion search strategy.
std::function<BooleanOrIntegerLiteral()>
ConstructIntegerCompletionSearchStrategy(
    absl::Span<const IntegerVariable> variable_mapping,
    IntegerVariable objective_var, Model* model);

// Constructs a search strategy that follows the hints from the model.
std::function<BooleanOrIntegerLiteral()> ConstructHintSearchStrategy(
    const CpModelProto& cp_model_proto, CpModelMapping* mapping, Model* model);

// Constructs our "fixed" search strategy which start with
// ConstructUserSearchStrategy() if present, but is completed by a couple of
// automatic heuristics.
void ConstructFixedSearchStrategy(SearchHeuristics* h, Model* model);

// For debugging fixed-search: display information about the named variables
// domain before taking each decision. Note that we copy the instrumented
// strategy so it doesn't have to outlive the returned functions like the other
// arguments.
std::function<BooleanOrIntegerLiteral()> InstrumentSearchStrategy(
    const CpModelProto& cp_model_proto,
    absl::Span<const IntegerVariable> variable_mapping,
    std::function<BooleanOrIntegerLiteral()> instrumented_strategy,
    Model* model);

// Returns all the named set of parameters known to the solver. This include our
// default strategies like "max_lp", "core", etc... It is visible here so that
// this can be reused by parameter validation.
//
// Usually, named strategies just override a few field from the base_params.
absl::flat_hash_map<std::string, SatParameters> GetNamedParameters(
    SatParameters base_params);

// Returns a list of full workers to run.
class SubsolverNameFilter;
std::vector<SatParameters> GetFullWorkerParameters(
    const SatParameters& base_params, const CpModelProto& cp_model,
    int num_already_present, SubsolverNameFilter* name_filter);

// Given a base set of parameter, if non-empty, this repeat them (round-robbin)
// until we get num_params_to_generate. Note that if we don't have a multiple,
// the first base parameters will be repeated more than the others.
//
// Note that this will also change the random_seed of each of these parameters.
std::vector<SatParameters> RepeatParameters(
    absl::Span<const SatParameters> base_params, int num_params_to_generate);

// Returns a vector of base parameters to specify solvers specialized to find a
// initial solution. This is meant to be used with RepeatParameters() and
// FilterParameters().
std::vector<SatParameters> GetFirstSolutionBaseParams(
    const SatParameters& base_params);

// Simple class used to filter executed subsolver names.
class SubsolverNameFilter {
 public:
  // Warning, params must outlive the class and be constant.
  explicit SubsolverNameFilter(const SatParameters& params);

  // Shall we keep a parameter with given name?
  bool Keep(absl::string_view name);

  // Applies Keep() to all the input list.
  std::vector<SatParameters> Filter(absl::Span<const SatParameters> input) {
    std::vector<SatParameters> result;
    for (const SatParameters& param : input) {
      if (Keep(param.name())) {
        result.push_back(param);
      }
    }
    return result;
  }

  // This is just a convenient function to follow the pattern
  // if (filter.Keep("my_name")) subsovers.Add(.... filter.LastName() ... )
  // And not repeat "my_name" twice.
  std::string LastName() const { return last_name_; }

  // Returns the list of all ignored subsolver for use in logs.
  const std::vector<std::string>& AllIgnored() {
    gtl::STLSortAndRemoveDuplicates(&ignored_);
    return ignored_;
  }

 private:
  // Copy of absl::log_internal::FNMatch().
  bool FNMatch(absl::string_view pattern, absl::string_view str);

  std::vector<std::string> filter_patterns_;
  std::vector<std::string> ignore_patterns_;
  std::string last_name_;

  std::vector<std::string> ignored_;
};

}  // namespace sat
}  // namespace operations_research

#endif  // ORTOOLS_SAT_CP_MODEL_SEARCH_H_
