// Copyright 2010-2024 Google LLC
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

#ifndef OR_TOOLS_SAT_LINEAR_CONSTRAINT_MANAGER_H_
#define OR_TOOLS_SAT_LINEAR_CONSTRAINT_MANAGER_H_

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "absl/container/btree_map.h"
#include "absl/container/flat_hash_map.h"
#include "absl/strings/string_view.h"
#include "ortools/base/strong_vector.h"
#include "ortools/glop/variables_info.h"
#include "ortools/lp_data/lp_types.h"
#include "ortools/sat/integer.h"
#include "ortools/sat/linear_constraint.h"
#include "ortools/sat/model.h"
#include "ortools/sat/sat_parameters.pb.h"
#include "ortools/sat/util.h"
#include "ortools/util/logging.h"
#include "ortools/util/strong_integers.h"
#include "ortools/util/time_limit.h"

namespace operations_research {
namespace sat {

// Stores for each IntegerVariable its temporary LP solution.
//
// This is shared between all LinearProgrammingConstraint because in the corner
// case where we have many different LinearProgrammingConstraint and a lot of
// variable, we could theoretically use up a quadratic amount of memory
// otherwise.
struct ModelLpValues
    : public util_intops::StrongVector<IntegerVariable, double> {
  ModelLpValues() = default;
};

// This class holds a list of globally valid linear constraints and has some
// logic to decide which one should be part of the LP relaxation. We want more
// for a better relaxation, but for efficiency we do not want to have too much
// constraints while solving the LP.
//
// This class is meant to contain all the initial constraints of the LP
// relaxation and to get new cuts as they are generated. Thus, it can both
// manage cuts but also only add the initial constraints lazily if there is too
// many of them.
class LinearConstraintManager {
 public:
  struct ConstraintInfo {
    // Note that this constraint always contains "tight" lb/ub, some of these
    // bound might be trivial level zero bounds, and one can know this by
    // looking at lb_is_trivial/ub_is_trivial.
    LinearConstraint constraint;

    double l2_norm = 0.0;
    double objective_parallelism = 0.0;
    size_t hash;

    // Updated only for deletable constraints. This is incremented every time
    // ChangeLp() is called and the constraint is active in the LP or not in the
    // LP and violated.
    double active_count = 0.0;

    // TODO(user): This is the number of time the constraint was consecutively
    // inactive, and go up to 100 with the default param, so we could optimize
    // the space used more.
    uint16_t inactive_count = 0;

    // TODO(user): Pack bool and in general optimize the memory of this class.
    bool objective_parallelism_computed = false;
    bool is_in_lp = false;
    bool ub_is_trivial = false;
    bool lb_is_trivial = false;

    // For now, we mark all the generated cuts as deletable and the problem
    // constraints as undeletable.
    // TODO(user): We can have a better heuristics. Some generated good cuts
    // can be marked undeletable and some unused problem specified constraints
    // can be marked deletable.
    bool is_deletable = false;
  };

  explicit LinearConstraintManager(Model* model)
      : sat_parameters_(*model->GetOrCreate<SatParameters>()),
        integer_trail_(*model->GetOrCreate<IntegerTrail>()),
        time_limit_(model->GetOrCreate<TimeLimit>()),
        expanded_lp_solution_(*model->GetOrCreate<ModelLpValues>()),
        model_(model),
        logger_(model->GetOrCreate<SolverLogger>()) {}
  ~LinearConstraintManager();

  // Add a new constraint to the manager. Note that we canonicalize constraints
  // and merge the bounds of constraints with the same terms. We also perform
  // basic preprocessing. If added is given, it will be set to true if this
  // constraint was actually a new one and to false if it was dominated by an
  // already existing one.
  DEFINE_STRONG_INDEX_TYPE(ConstraintIndex);
  ConstraintIndex Add(LinearConstraint ct, bool* added = nullptr);

  // Same as Add(), but logs some information about the newly added constraint.
  // Cuts are also handled slightly differently than normal constraints.
  //
  // Returns true if a new cut was added and false if this cut is not
  // efficacious or if it is a duplicate of an already existing one.
  bool AddCut(LinearConstraint ct, std::string type_name,
              std::string extra_info = "");

  // These must be level zero bounds.
  bool UpdateConstraintLb(glop::RowIndex index_in_lp, IntegerValue new_lb);
  bool UpdateConstraintUb(glop::RowIndex index_in_lp, IntegerValue new_ub);

  // The objective is used as one of the criterion to score cuts.
  // The more a cut is parallel to the objective, the better its score is.
  //
  // Currently this should only be called once per IntegerVariable (Checked). It
  // is easy to support dynamic modification if it becomes needed.
  void SetObjectiveCoefficient(IntegerVariable var, IntegerValue coeff);

  // Heuristic to decides what LP is best solved next. We use the model lp
  // solutions as an heuristic, and it should usually be updated with the last
  // known solution before this call.
  //
  // The current solution state is used for detecting inactive constraints. It
  // is also updated correctly on constraint deletion/addition so that the
  // simplex can be fully iterative on restart by loading this modified state.
  //
  // Returns true iff LpConstraints() will return a different LP than before.
  bool ChangeLp(glop::BasisState* solution_state,
                int* num_new_constraints = nullptr);

  // This can be called initially to add all the current constraint to the LP
  // returned by GetLp().
  void AddAllConstraintsToLp();

  // All the constraints managed by this class.
  const util_intops::StrongVector<ConstraintIndex, ConstraintInfo>&
  AllConstraints() const {
    return constraint_infos_;
  }

  // The set of constraints indices in AllConstraints() that should be part
  // of the next LP to solve.
  const std::vector<ConstraintIndex>& LpConstraints() const {
    return lp_constraints_;
  }

  // To simplify CutGenerator api.
  const util_intops::StrongVector<IntegerVariable, double>& LpValues() {
    return expanded_lp_solution_;
  }

  // Stats.
  int64_t num_constraints() const { return constraint_infos_.size(); }
  int64_t num_constraint_updates() const { return num_constraint_updates_; }
  int64_t num_simplifications() const { return num_simplifications_; }
  int64_t num_merged_constraints() const { return num_merged_constraints_; }
  int64_t num_shortened_constraints() const {
    return num_shortened_constraints_;
  }
  int64_t num_split_constraints() const { return num_split_constraints_; }
  int64_t num_coeff_strenghtening() const { return num_coeff_strenghtening_; }
  int64_t num_cuts() const { return num_cuts_; }
  int64_t num_add_cut_calls() const { return num_add_cut_calls_; }

  const absl::btree_map<std::string, int>& type_to_num_cuts() const {
    return type_to_num_cuts_;
  }

  // If a debug solution has been loaded, this checks if the given constraint
  // cut it or not. Returns true if and only if everything is fine and the cut
  // does not violate the loaded solution.
  bool DebugCheckConstraint(const LinearConstraint& cut);

 private:
  // Heuristic that decide which constraints we should remove from the current
  // LP. Note that such constraints can be added back later by the heuristic
  // responsible for adding new constraints from the pool.
  //
  // Returns true if and only if one or more constraints where removed.
  //
  // If the solutions_state is empty, then this function does nothing and
  // returns false (this is used for tests). Otherwise, the solutions_state is
  // assumed to correspond to the current LP and to be of the correct size.
  bool MaybeRemoveSomeInactiveConstraints(glop::BasisState* solution_state);

  // Apply basic inprocessing simplification rules:
  //  - remove fixed variable
  //  - reduce large coefficient (i.e. coeff strenghtenning or big-M reduction).
  // This uses level-zero bounds.
  // Returns true if the terms of the constraint changed.
  bool SimplifyConstraint(LinearConstraint* ct);

  // Helper method to compute objective parallelism for a given constraint. This
  // also lazily computes objective norm.
  void ComputeObjectiveParallelism(ConstraintIndex ct_index);

  // Multiplies all active counts and the increment counter by the given
  // 'scaling_factor'. This should be called when at least one of the active
  // counts is too high.
  void RescaleActiveCounts(double scaling_factor);

  // Removes some deletable constraints with low active counts. For now, we
  // don't remove any constraints which are already in LP.
  void PermanentlyRemoveSomeConstraints();

  // Make sure the lb/ub are tight and fill lb_is_trivial/ub_is_trivial.
  void FillDerivedFields(ConstraintInfo* info);

  const SatParameters& sat_parameters_;
  const IntegerTrail& integer_trail_;

  // Set at true by Add()/SimplifyConstraint() and at false by ChangeLp().
  bool current_lp_is_changed_ = false;

  // Optimization to avoid calling SimplifyConstraint() when not needed.
  int64_t last_simplification_timestamp_ = 0;

  util_intops::StrongVector<ConstraintIndex, ConstraintInfo> constraint_infos_;

  // The subset of constraints currently in the lp.
  std::vector<ConstraintIndex> lp_constraints_;

  // We keep a map from the hash of our constraint terms to their position in
  // constraints_. This is an optimization to detect duplicate constraints. We
  // are robust to collisions because we always relies on the ground truth
  // contained in constraints_ and the code is still okay if we do not merge the
  // constraints.
  absl::flat_hash_map<size_t, ConstraintIndex> equiv_constraints_;

  int64_t num_constraint_updates_ = 0;
  int64_t num_simplifications_ = 0;
  int64_t num_merged_constraints_ = 0;
  int64_t num_shortened_constraints_ = 0;
  int64_t num_split_constraints_ = 0;
  int64_t num_coeff_strenghtening_ = 0;

  int64_t num_cuts_ = 0;
  int64_t num_add_cut_calls_ = 0;
  absl::btree_map<std::string, int> type_to_num_cuts_;

  bool objective_is_defined_ = false;
  bool objective_norm_computed_ = false;
  double objective_l2_norm_ = 0.0;

  // Total deterministic time spent in this class.
  double dtime_ = 0.0;

  // Sparse representation of the objective coeffs indexed by positive variables
  // indices. Important: We cannot use a dense representation here in the corner
  // case where we have many independent LPs. Alternatively, we could share a
  // dense vector between all LinearConstraintManager.
  double sum_of_squared_objective_coeffs_ = 0.0;
  absl::flat_hash_map<IntegerVariable, double> objective_map_;

  TimeLimit* time_limit_;
  ModelLpValues& expanded_lp_solution_;
  Model* model_;
  SolverLogger* logger_;

  // We want to decay the active counts of all constraints at each call and
  // increase the active counts of active/violated constraints. However this can
  // be too slow in practice. So instead, we keep an increment counter and
  // update only the active/violated constraints. The counter itself is
  // increased by a factor at each call. This has the same effect as decaying
  // all the active counts at each call. This trick is similar to sat clause
  // management.
  double constraint_active_count_increase_ = 1.0;

  int32_t num_deletable_constraints_ = 0;
};

// Before adding cuts to the global pool, it is a classical thing to only keep
// the top n of a given type during one generation round. This is there to help
// doing that.
//
// TODO(user): Avoid computing efficacity twice.
// TODO(user): We don't use any orthogonality consideration here.
// TODO(user): Detect duplicate cuts?
class TopNCuts {
 public:
  explicit TopNCuts(int n) : cuts_(n) {}

  // Adds a cut to the local pool.
  void AddCut(
      LinearConstraint ct, absl::string_view name,
      const util_intops::StrongVector<IntegerVariable, double>& lp_solution);

  // Empty the local pool and add all its content to the manager.
  void TransferToManager(LinearConstraintManager* manager);

 private:
  struct CutCandidate {
    std::string name;
    LinearConstraint cut;
  };
  TopN<CutCandidate, double> cuts_;
};

}  // namespace sat
}  // namespace operations_research

#endif  // OR_TOOLS_SAT_LINEAR_CONSTRAINT_MANAGER_H_
