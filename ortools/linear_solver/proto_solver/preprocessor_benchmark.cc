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

#include <vector>

#include "absl/types/span.h"
#include "benchmark/benchmark.h"
#include "ortools/linear_solver/linear_solver.pb.h"
#include "ortools/linear_solver/proto_solver/preprocessor.h"
#include "ortools/lp_data/lp_types.h"
#include "ortools/sat/mp_model_wrapper.h"

namespace operations_research {
namespace {

using ::operations_research::glop::kInfinity;

// Benchmark bound propagators for PropagateBounds() (uses Kahan sum,
// does not change rounding modes). The model we use here is of form:
// for i = 1..range(0):
//   model.add("a_i >= 0;
//              a_i - b_i + sum_{j=1..range(1)} c_j <= 0;
//              a_i - b_i + sum_{j=1..range(1)} c_j >= 1;")
// where a_i and b_i are integer variables (could be continuous too, doesn't
// matter), and c_j are all variables fixed to 0.0 which are here just to make
// the constraints larger (otherwise, we'll be unable to detect benefits
// stemming from incrementality).
// Note that this formulation is unbounded, and a_i and b_i will
// propagate bounds between each other (potentially to infinity). This is
// because:
//   1. a_i >= 0.0 propagates b_i >= 0 (via a_i - b_i + ... <= 0).
//   2. b_i >= 0.0 propagates a_i >= 1 (via a_i - b_i + ... >= 1).
//   3. etc.
// Consequently, this model will trigger many propagations, which is good for
// benchmarking.
void BM_Propagation(benchmark::State& state) {
  MPModelProto model;

  std::vector<int> var_indices;
  const int kNumEmptyVariables = state.range(1);
  for (int j = 0; j < kNumEmptyVariables; ++j) {
    const int c = model.variable_size();
    MPVariableProto* var = model.add_variable();
    var->set_lower_bound(0.0);
    var->set_upper_bound(0.0);
    var_indices.push_back(c);
  }

  const int kNumVariablePairs = state.range(0);
  for (int i = 0; i < kNumVariablePairs; ++i) {
    const int a = model.variable_size();
    MPVariableProto* var_a = model.add_variable();
    var_a->set_lower_bound(0.0);
    var_a->set_upper_bound(kInfinity);
    var_a->set_is_integer(true);
    const int b = model.variable_size();
    MPVariableProto* var_b = model.add_variable();
    var_b->set_lower_bound(-kInfinity);
    var_b->set_upper_bound(kInfinity);
    var_b->set_is_integer(true);
    MPConstraintProto* ct1 = model.add_constraint();
    ct1->add_var_index(a);
    ct1->add_coefficient(1.0);
    ct1->add_var_index(b);
    ct1->add_coefficient(-1.0);
    for (const int c : var_indices) {
      ct1->add_var_index(c);
      ct1->add_coefficient(1.0);
    }
    ct1->set_lower_bound(-kInfinity);
    ct1->set_upper_bound(0.0);
    MPConstraintProto* ct2 = model.add_constraint();
    ct2->add_var_index(a);
    ct2->add_coefficient(1.0);
    ct2->add_var_index(b);
    ct2->add_coefficient(-1.0);
    ct2->set_lower_bound(1.0);
    ct2->set_upper_bound(kInfinity);
    for (const int c : var_indices) {
      ct2->add_var_index(c);
      ct2->add_coefficient(1.0);
    }
  }
  sat::MPModelProtoWrapper wrapper(&model, nullptr);
  const int num_variables = wrapper.num_variables();
  std::vector<double> var_lbs(num_variables);
  std::vector<double> var_ubs(num_variables);
  for (int v = 0; v < num_variables; ++v) {
    var_lbs[v] = wrapper.variables(v).lower_bound();
    var_ubs[v] = wrapper.variables(v).upper_bound();
  }
  while (state.KeepRunning()) {
    [[maybe_unused]] const bool result = PropagateBounds(
        wrapper, absl::MakeSpan(var_lbs), absl::MakeSpan(var_ubs));
  }
}

BENCHMARK(BM_Propagation)
    ->ArgPair(10, 1)
    ->ArgPair(100, 10)
    ->ArgPair(1000, 100)
    ->ArgPair(1, 10)
    ->ArgPair(10, 100)
    ->ArgPair(100, 1000);

}  // namespace
}  // namespace operations_research
