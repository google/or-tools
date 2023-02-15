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

#ifndef OR_TOOLS_GLOP_ENTERING_VARIABLE_H_
#define OR_TOOLS_GLOP_ENTERING_VARIABLE_H_

#include <cstdint>
#include <string>
#include <vector>

#include "absl/random/bit_gen_ref.h"
#include "ortools/glop/basis_representation.h"
#include "ortools/glop/parameters.pb.h"
#include "ortools/glop/primal_edge_norms.h"
#include "ortools/glop/reduced_costs.h"
#include "ortools/glop/status.h"
#include "ortools/glop/update_row.h"
#include "ortools/glop/variables_info.h"
#include "ortools/lp_data/lp_data.h"
#include "ortools/lp_data/lp_types.h"
#include "ortools/util/bitset.h"
#include "ortools/util/stats.h"

#if !SWIG

namespace operations_research {
namespace glop {

// This class contains the dual algorithms that choose the entering column (i.e.
// variable) during a dual simplex iteration. That is the dual ratio test.
//
// Terminology:
// - The entering edge is the edge we are following during a simplex step,
//   and we call "direction" the reverse of this edge restricted to the
//   basic variables, i.e. the right inverse of the entering column.
class EnteringVariable {
 public:
  // Takes references to the linear program data we need.
  EnteringVariable(const VariablesInfo& variables_info, absl::BitGenRef random,
                   ReducedCosts* reduced_costs);

  // Dual optimization phase (i.e. phase II) ratio test.
  // Returns the index of the entering column given that we want to move along
  // the "update" row vector in the direction given by the sign of
  // cost_variation. Computes the smallest step that keeps the dual feasibility
  // for all the columns.
  ABSL_MUST_USE_RESULT Status DualChooseEnteringColumn(
      bool nothing_to_recompute, const UpdateRow& update_row,
      Fractional cost_variation, std::vector<ColIndex>* bound_flip_candidates,
      ColIndex* entering_col);

  // Dual feasibility phase (i.e. phase I) ratio test.
  // Similar to the optimization phase test, but allows a step that increases
  // the infeasibility of an already infeasible column. The step magnitude is
  // the one that minimize the sum of infeasibilities when applied.
  ABSL_MUST_USE_RESULT Status DualPhaseIChooseEnteringColumn(
      bool nothing_to_recompute, const UpdateRow& update_row,
      Fractional cost_variation, ColIndex* entering_col);

  // Sets the parameters.
  void SetParameters(const GlopParameters& parameters);

  // Stats related functions.
  std::string StatString() const { return stats_.StatString(); }

  // Deterministic time used by some of the functions of this class.
  //
  // TODO(user): Be exhausitive and more precise.
  double DeterministicTime() const {
    return DeterministicTimeForFpOperations(num_operations_);
  }

 private:
  // Problem data that should be updated from outside.
  const VariablesInfo& variables_info_;

  absl::BitGenRef random_;
  ReducedCosts* reduced_costs_;

  // Internal data.
  GlopParameters parameters_;

  // Stats.
  struct Stats : public StatsGroup {
    Stats()
        : StatsGroup("EnteringVariable"),
          num_perfect_ties("num_perfect_ties", this) {}
    IntegerDistribution num_perfect_ties;
  };
  Stats stats_;

  // Temporary vector used to hold the best entering column candidates that are
  // tied using the current choosing criteria. We actually only store the tied
  // candidate #2, #3, ...; because the first tied candidate is remembered
  // anyway.
  std::vector<ColIndex> equivalent_entering_choices_;

  // Store a column with its update coefficient and ratio.
  // This is used during the dual phase I & II ratio tests.
  struct ColWithRatio {
    ColWithRatio() = default;
    ColWithRatio(ColIndex _col, Fractional reduced_cost, Fractional coeff_m)
        : col(_col), ratio(reduced_cost / coeff_m), coeff_magnitude(coeff_m) {}

    // Returns false if "this" is before "other" in a priority queue.
    bool operator<(const ColWithRatio& other) const {
      if (ratio == other.ratio) {
        if (coeff_magnitude == other.coeff_magnitude) {
          return col > other.col;
        }
        return coeff_magnitude < other.coeff_magnitude;
      }
      return ratio > other.ratio;
    }

    ColIndex col;
    Fractional ratio;
    Fractional coeff_magnitude;
  };

  // Temporary vector used to hold breakpoints.
  std::vector<ColWithRatio> breakpoints_;

  // Counter for the deterministic time.
  int64_t num_operations_ = 0;

  DISALLOW_COPY_AND_ASSIGN(EnteringVariable);
};

}  // namespace glop
}  // namespace operations_research

#endif  // SWIG
#endif  // OR_TOOLS_GLOP_ENTERING_VARIABLE_H_
