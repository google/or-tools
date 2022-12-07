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

#ifndef OR_TOOLS_MATH_OPT_SOLVERS_PDLP_BRIDGE_H_
#define OR_TOOLS_MATH_OPT_SOLVERS_PDLP_BRIDGE_H_

#include <cstdint>
#include <optional>
#include <vector>

#include "Eigen/Core"
#include "absl/container/flat_hash_map.h"
#include "absl/status/statusor.h"
#include "ortools/math_opt/core/inverted_bounds.h"
#include "ortools/math_opt/model.pb.h"
#include "ortools/math_opt/model_parameters.pb.h"
#include "ortools/math_opt/sparse_containers.pb.h"
#include "ortools/pdlp/primal_dual_hybrid_gradient.h"
#include "ortools/pdlp/quadratic_program.h"

namespace operations_research {
namespace math_opt {

// Builds a PDLP model (QuadraticProgram) from ModelProto, and provides methods
// to translate solutions back and forth.
//
// The primary difference in the models are:
//  1. PDLP maps the variable/constraint ids to consecutive indices
//     [0, 1, ..., n).
//  2. PDLP does not support maximization. If the ModelProto is a maximization
//     problem, the objective is negated (coefficients and offset) before
//     passing to PDLP. On the way back, the objective value, and all dual
//     variables/reduced costs (also for rays) must be negated.
//
// Throughout, it is assumed that the MathOpt protos have been validated, but
// no assumption is made on the PDLP output. Any Status errors resulting from
// invalid PDLP output use the status code kInternal.
class PdlpBridge {
 public:
  PdlpBridge() = default;
  static absl::StatusOr<PdlpBridge> FromProto(const ModelProto& model_proto);

  const pdlp::QuadraticProgram& pdlp_lp() const { return pdlp_lp_; }

  // Returns the ids of variables and linear constraints with inverted bounds.
  InvertedBounds ListInvertedBounds() const;

  // TODO(b/183616124): we need to support the inverse of these methods for
  // warm start.
  absl::StatusOr<SparseDoubleVectorProto> PrimalVariablesToProto(
      const Eigen::VectorXd& primal_values,
      const SparseVectorFilterProto& variable_filter) const;
  absl::StatusOr<SparseDoubleVectorProto> DualVariablesToProto(
      const Eigen::VectorXd& dual_values,
      const SparseVectorFilterProto& linear_constraint_filter) const;
  absl::StatusOr<SparseDoubleVectorProto> ReducedCostsToProto(
      const Eigen::VectorXd& reduced_costs,
      const SparseVectorFilterProto& variable_filter) const;
  pdlp::PrimalAndDualSolution SolutionHintToWarmStart(
      const SolutionHintProto& solution_hint) const;

 private:
  pdlp::QuadraticProgram pdlp_lp_;
  absl::flat_hash_map<int64_t, int64_t> var_id_to_pdlp_index_;
  // NOTE: this vector is strictly increasing
  std::vector<int64_t> pdlp_index_to_var_id_;
  absl::flat_hash_map<int64_t, int64_t> lin_con_id_to_pdlp_index_;
  // NOTE: this vector is strictly increasing
  std::vector<int64_t> pdlp_index_to_lin_con_id_;
};

}  // namespace math_opt
}  // namespace operations_research

#endif  // OR_TOOLS_MATH_OPT_SOLVERS_PDLP_BRIDGE_H_
