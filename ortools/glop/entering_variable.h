// Copyright 2010-2017 Google
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
#include "ortools/util/random_engine.h"
#include "ortools/util/stats.h"

#if !SWIG

namespace operations_research {
namespace glop {

// This class contains the primal and dual algorithms that choose the entering
// column (i.e. variable) during a simplex iteration.
//
// The goal of this component is, given a matrix A (matrix_), a subset of
// columns (basis_) that form a basis B and a cost (objective_) associated to
// each column of A, to choose a "good" non-basic column to enter the basis
// B. Note that this choice does not depend on the current variable values
// (except for the direction in which each variable is allowed to change given
// by variable_status_).
//
// Terminology:
// - The entering edge is the edge we are following during a simplex step,
//   and we call "direction" the reverse of this edge restricted to the
//   basic variables, i.e. the right inverse of the entering column.
//
// Papers:
// - Ping-Qi Pan, "Efficient nested pricing in the simplex algorithm",
//   http://www.optimization-online.org/DB_FILE/2007/10/1810.pdf
class EnteringVariable {
 public:
  // Takes references to the linear program data we need.
  EnteringVariable(const VariablesInfo& variables_info, random_engine_t* random,
                   ReducedCosts* reduced_costs,
                   PrimalEdgeNorms* primal_edge_norms);

  // Returns the index of a valid primal entering column (see
  // IsValidPrimalEnteringCandidate() for more details) or kInvalidCol if no
  // such column exists. This latter case means that the primal algorithm has
  // terminated: the optimal has been reached.
  Status PrimalChooseEnteringColumn(ColIndex* entering_col) MUST_USE_RESULT;

  // Dual optimization phase (i.e. phase II) ratio test.
  // Returns the index of the entering column given that we want to move along
  // the "update" row vector in the direction given by the sign of
  // cost_variation. Computes the smallest step that keeps the dual feasibility
  // for all the columns. The pivot is the coefficient of the "update" vector at
  // the entering column index.
  Status DualChooseEnteringColumn(const UpdateRow& update_row,
                                  Fractional cost_variation,
                                  std::vector<ColIndex>* bound_flip_candidates,
                                  ColIndex* entering_col, Fractional* pivot,
                                  Fractional* step) MUST_USE_RESULT;

  // Dual feasibility phase (i.e. phase I) ratio test.
  // Similar to the optimization phase test, but allows a step that increases
  // the infeasibility of an already infeasible column. The step magnitude is
  // the one that minimize the sum of infeasibilities when applied.
  Status DualPhaseIChooseEnteringColumn(const UpdateRow& update_row,
                                        Fractional cost_variation,
                                        ColIndex* entering_col,
                                        Fractional* pivot,
                                        Fractional* step) MUST_USE_RESULT;

  // Sets the pricing parameters. This does not change the pricing rule.
  void SetParameters(const GlopParameters& parameters);

  // Sets the pricing rule.
  void SetPricingRule(GlopParameters::PricingRule rule);

  // Stats related functions.
  std::string StatString() const { return stats_.StatString(); }

  // Recomputes the set of unused columns used during nested pricing.
  // Visible for testing (the returns value is also there for testing).
  DenseBitRow* ResetUnusedColumns();

 private:
  // Dantzig selection rule: choose the variable with the best reduced cost.
  // If normalize is true, we normalize the costs by the column norms.
  // If nested_pricing is true, we use nested pricing (see parameters.proto).
  template <bool normalize, bool nested_pricing>
  void DantzigChooseEnteringColumn(ColIndex* entering_col);

  // Steepest edge rule: the reduced costs are normalized by the edges norm.
  // Devex rule: the reduced costs are normalized by an approximation of the
  // edges norm.
  template <bool use_steepest_edge>
  void NormalizedChooseEnteringColumn(ColIndex* entering_col);

  // Problem data that should be updated from outside.
  const VariablesInfo& variables_info_;

  random_engine_t* random_;
  ReducedCosts* reduced_costs_;
  PrimalEdgeNorms* primal_edge_norms_;

  // Internal data.
  GlopParameters parameters_;
  GlopParameters::PricingRule rule_;

  // Stats.
  struct Stats : public StatsGroup {
    Stats()
        : StatsGroup("EnteringVariable"),
          num_perfect_ties("num_perfect_ties", this) {}
    IntegerDistribution num_perfect_ties;
  };
  Stats stats_;

  // This is used for nested pricing. It is denoted J in Ping-Qi Pan's paper.
  // At a given step, it is true for the variable that should be considered for
  // entering the basis.
  DenseBitRow unused_columns_;

  // Temporary vector used to hold the best entering column candidates that are
  // tied using the current choosing criteria. We actually only store the tied
  // candidate #2, #3, ...; because the first tied candidate is remembered
  // anyway.
  std::vector<ColIndex> equivalent_entering_choices_;

  // Store a column with its update coefficient and ratio.
  // This is used during the dual phase I & II ratio tests.
  struct ColWithRatio {
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

  DISALLOW_COPY_AND_ASSIGN(EnteringVariable);
};

}  // namespace glop
}  // namespace operations_research

#endif  // SWIG
#endif  // OR_TOOLS_GLOP_ENTERING_VARIABLE_H_
