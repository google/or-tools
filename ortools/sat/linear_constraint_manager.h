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

#ifndef OR_TOOLS_SAT_LINEAR_CONSTRAINT_MANAGER_H_
#define OR_TOOLS_SAT_LINEAR_CONSTRAINT_MANAGER_H_

#include <vector>

#include "absl/container/flat_hash_map.h"
#include "absl/container/flat_hash_set.h"
#include "ortools/sat/linear_constraint.h"
#include "ortools/sat/model.h"
#include "ortools/sat/sat_parameters.pb.h"

namespace operations_research {
namespace sat {

// This class holds a list of globally valid linear constraints and has some
// logic to decide which one should be part of the LP relaxation. We want more
// for a better relaxation, but for efficiency we do not want to have too much
// constraints while solving the LP.
//
// This class is meant to contain all the initial constraints of the LP
// relaxation and to get new cuts as they are generated. Thus, it can both
// manage cuts but also only add the initial constraints lazily if there is too
// many of them.
//
// TODO(user): Also store the LP objective there as it can be useful to decide
// which constraint should go into the current LP.
class LinearConstraintManager {
 public:
  explicit LinearConstraintManager(Model* model)
      : sat_parameters_(*model->GetOrCreate<SatParameters>()),
        integer_trail_(*model->GetOrCreate<IntegerTrail>()) {}
  ~LinearConstraintManager();

  // Add a new constraint to the manager. Note that we canonicalize constraints
  // and merge the bounds of constraints with the same terms. We also perform
  // basic preprocessing.
  DEFINE_INT_TYPE(ConstraintIndex, int32);
  ConstraintIndex Add(LinearConstraint ct);

  // Same as Add(), but logs some information about the newly added constraint.
  // Cuts are also handled slightly differently than normal constraints.
  void AddCut(LinearConstraint ct, std::string type_name,
              const gtl::ITIVector<IntegerVariable, double>& lp_solution);

  // Heuristic to decides what LP is best solved next. The given lp_solution
  // should usually be the optimal solution of the LP returned by GetLp() before
  // this call, but is just used as an heuristic.
  //
  // Returns true iff LpConstraints() will return a different LP than before.
  bool ChangeLp(const gtl::ITIVector<IntegerVariable, double>& lp_solution);

  // This can be called initially to add all the current constraint to the LP
  // returned by GetLp().
  void AddAllConstraintsToLp();

  // All the constraints managed by this class.
  const gtl::ITIVector<ConstraintIndex, LinearConstraint>& AllConstraints()
      const {
    return constraints_;
  }

  // The set of constraints indices in AllConstraints() that should be part
  // of the next LP to solve.
  const std::vector<ConstraintIndex>& LpConstraints() const {
    return lp_constraints_;
  }

  int64 num_cuts() const { return num_cuts_; }
  int64 num_shortened_constraints() const { return num_shortened_constraints_; }
  int64 num_coeff_strenghtening() const { return num_coeff_strenghtening_; }

 private:
  // Removes the marked constraints from the LP.
  void RemoveMarkedConstraints();

  // Apply basic inprocessing simplification rules:
  //  - remove fixed variable
  //  - reduce large coefficient (i.e. coeff strenghtenning or big-M reduction).
  // This uses level-zero bounds.
  // Returns true if the terms of the constraint changed.
  bool SimplifyConstraint(LinearConstraint* ct);

  const SatParameters& sat_parameters_;
  const IntegerTrail& integer_trail_;

  // Set at true by Add()/SimplifyConstraint() and at false by ChangeLp().
  bool current_lp_is_changed_ = false;

  // Optimization to avoid calling SimplifyConstraint() when not needed.
  int64 last_simplification_timestamp_ = 0;

  // TODO(user): Merge all the constraint related info in a struct and store
  // a vector of struct instead. The global list of constraint.
  gtl::ITIVector<ConstraintIndex, LinearConstraint> constraints_;
  gtl::ITIVector<ConstraintIndex, double> constraint_l2_norms_;
  gtl::ITIVector<ConstraintIndex, bool> constraint_is_cut_;
  gtl::ITIVector<ConstraintIndex, int64> constraint_inactive_count_;

  // Temporary list of constraints marked for removal. Note that we remove
  // constraints in batch to avoid changing LP too frequently.
  absl::flat_hash_set<ConstraintIndex> constraints_removal_list_;

  // List of permananently removed constraints. A constraint might be marked for
  // permanent removal if it is almost parallel to one of the existing
  // constraints in the LP.
  gtl::ITIVector<ConstraintIndex, bool> constraint_permanently_removed_;

  // The subset of constraints currently in the lp.
  gtl::ITIVector<ConstraintIndex, bool> constraint_is_in_lp_;
  std::vector<ConstraintIndex> lp_constraints_;

  // We keep a map from the hash of our constraint terms to their position in
  // constraints_. This is an optimization to detect duplicate constraints. We
  // are robust to collisions because we always relies on the ground truth
  // contained in constraints_ and the code is still okay if we do not merge the
  // constraints.
  absl::flat_hash_map<size_t, ConstraintIndex> equiv_constraints_;
  gtl::ITIVector<ConstraintIndex, size_t> constraint_hashes_;

  int64 num_merged_constraints_ = 0;
  int64 num_shortened_constraints_ = 0;
  int64 num_splitted_constraints_ = 0;
  int64 num_coeff_strenghtening_ = 0;

  int64 num_cuts_ = 0;
  std::map<std::string, int> type_to_num_cuts_;
};

}  // namespace sat
}  // namespace operations_research

#endif  // OR_TOOLS_SAT_LINEAR_CONSTRAINT_MANAGER_H_
