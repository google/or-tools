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

#ifndef ORTOOLS_SAT_LINEAR_CONSTRAINT_MANAGER_H_
#define ORTOOLS_SAT_LINEAR_CONSTRAINT_MANAGER_H_

#include <cstddef>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

#include "absl/container/btree_map.h"
#include "absl/container/flat_hash_map.h"
#include "absl/log/check.h"
#include "absl/numeric/int128.h"
#include "absl/strings/string_view.h"
#include "absl/types/span.h"
#include "ortools/base/strong_vector.h"
#include "ortools/glop/variables_info.h"
#include "ortools/lp_data/lp_types.h"
#include "ortools/sat/clause.h"
#include "ortools/sat/integer.h"
#include "ortools/sat/integer_base.h"
#include "ortools/sat/linear_constraint.h"
#include "ortools/sat/model.h"
#include "ortools/sat/sat_literal.h"
#include "ortools/sat/sat_parameters.pb.h"
#include "ortools/sat/synchronization.h"
#include "ortools/sat/util.h"
#include "ortools/util/strong_integers.h"
#include "ortools/util/time_limit.h"

namespace operations_research {
namespace sat {

// Knowing the symmetry of the IP problem should allow us to
// solve the LP faster via "folding" techniques.
//
// You can read this for the LP part: "Dimension Reduction via Colour
// Refinement", Martin Grohe, Kristian Kersting, Martin Mladenov, Erkal
// Selman, https://arxiv.org/abs/1307.5697
//
// In the presence of symmetry, by considering all symmetric versions of a
// constraint and summing them, we can derive a new constraint using the sum
// of the variables on each orbit instead of the individual variables.
//
// For the integration in a MIP solver, I couldn't find many references. The way
// I did it here is to introduce for each orbit a variable representing the
// sum of the orbit variables. This allows representing the folded LP in terms
// of these variables (that are connected to the rest of the solver) and just
// reuse the full machinery.
//
// There are related info in "Orbital Shrinking", Matteo Fischetti & Leo
// Liberti, https://link.springer.com/chapter/10.1007/978-3-642-32147-4_6
class LinearConstraintSymmetrizer {
 public:
  explicit LinearConstraintSymmetrizer(Model* model)
      : shared_stats_(model->GetOrCreate<SharedStatistics>()),
        integer_trail_(model->GetOrCreate<IntegerTrail>()) {}
  ~LinearConstraintSymmetrizer();

  // This must be called with all orbits before we call FoldLinearConstraint().
  // Note that sum_var MUST not be in any of the orbits. All orbits must also be
  // disjoint.
  //
  // Precondition: All IntegerVariable must be positive.
  void AddSymmetryOrbit(IntegerVariable sum_var,
                        absl::Span<const IntegerVariable> orbit);

  // If there is no symmetry, we shouldn't bother calling the functions below.
  // Note that they will still work, but be no-op.
  bool HasSymmetry() const { return has_symmetry_; }

  // Accessors by orbit index in [0, num_orbits).
  int NumOrbits() const { return orbits_.size(); }
  IntegerVariable OrbitSumVar(int i) const { return orbit_sum_vars_[i]; }
  absl::Span<const IntegerVariable> Orbit(int i) const { return orbits_[i]; }

  // Returns the orbit number in [0, num_orbits) if var belongs to a non-trivial
  // orbit or if it is an "orbit_sum_var". Returns -1 otherwise.
  int OrbitIndex(IntegerVariable var) const;

  // Returns true iff var is one of the sum_var passed to AddSymmetryOrbit().
  bool IsOrbitSumVar(IntegerVariable var) const;

  // This will be only true for variables not appearing in any orbit and for
  // the orbit sum variables.
  bool AppearInFoldedProblem(IntegerVariable var) const;

  // Given a constraint on the "original" model variables, try to construct a
  // symmetric version of it using the orbit sum variables. This might fail if
  // we encounter integer overflow. Returns true on success. On failure, the
  // original constraints will not be usable.
  //
  // Preconditions: All IntegerVariable must be positive. And the constraint
  // lb/ub must be tight and not +/- int64_t max.
  bool FoldLinearConstraint(LinearConstraint* ct, bool* folded = nullptr);

 private:
  SharedStatistics* shared_stats_;
  IntegerTrail* integer_trail_;

  bool has_symmetry_ = false;
  int64_t num_overflows_ = 0;
  LinearConstraintBuilder builder_;

  // We index our vector by positive variables only.
  util_intops::StrongVector<PositiveOnlyIndex, int> var_to_orbit_index_;

  // Orbit info indexed by number in [0, num_orbits).
  std::vector<IntegerVariable> orbit_sum_vars_;
  CompactVectorVector<int, IntegerVariable> orbits_;
};

// During the search, the SAT solver can detect that two Boolean variables are
// equivalent (see BinaryImplicationGraph::DetectEquivalences()). This class
// keeps track of these equivalences in terms of the [0, 1] IntegerVariables
// that are "views" of these Booleans, so that we can substitute one by the
// other in the LP relaxation.
//
// This is a Model singleton shared by all the LinearConstraintManager (one per
// LP component) so that the work is only done once. Each client registers the
// variables it cares about together with its component id, and we only
// substitute a variable by another one from the same component since the
// representative must be a column of the same LP.
//
// Important: Substitutions are only valid at level zero (they come from
// equivalences that are level-zero facts) and can only grow. Clients should
// check version() to know if the substitution map changed.
class BooleanViewEquivalences {
 public:
  explicit BooleanViewEquivalences(Model* model);

  // Returns a new component id to use with RegisterVariable().
  int NewComponent() {
    substituted_vars_by_component_.emplace_back();
    return substituted_vars_by_component_.size() - 1;
  }

  // Registers a positive variable. Variables that are not in [0, 1] or that
  // have no Boolean view will never be substituted, so it is fine to register
  // all the variables of a component. Registering the same variable more than
  // once is a no-op.
  void RegisterVariable(int component, IntegerVariable positive_var);

  // Recomputes the substitution map if new equivalences were detected or if
  // new variables were registered. This is cheap if nothing changed. Returns
  // true if the map changed (the version is incremented in this case).
  bool Update();

  // Incremented each time the substitution map changes.
  int64_t version() const { return version_; }

  // If the given positive variable is substituted, returns true and fills
  // *expr with an affine expression such that var == expr. The expr.var is a
  // (possibly negated) variable of the same component that is never
  // substituted itself, expr.coeff is 1 and expr.constant is 0 or 1.
  bool GetSubstitution(IntegerVariable positive_var,
                       AffineExpression* expr) const {
    DCHECK(VariableIsPositive(positive_var));
    if (positive_var >= substitution_rep_.size()) return false;
    const IntegerVariable rep = substitution_rep_[positive_var];
    if (rep == kNoIntegerVariable) return false;
    if (expr != nullptr) {
      *expr = AffineExpression(rep, IntegerValue(1),
                               substitution_offset_[positive_var]);
    }
    return true;
  }

  // The list of currently substituted (positive) variables of a component.
  const std::vector<IntegerVariable>& SubstitutedVariables(
      int component) const {
    return substituted_vars_by_component_[component];
  }

  int64_t num_substituted_variables() const { return num_substituted_; }

 private:
  const SatParameters& params_;
  const IntegerTrail& integer_trail_;
  const IntegerEncoder& integer_encoder_;
  const BinaryImplicationGraph& implication_graph_;
  const LinearConstraintSymmetrizer& symmetrizer_;

  // All the registered variables, in registration order.
  std::vector<std::pair<int, IntegerVariable>> registered_vars_;
  util_intops::StrongVector<IntegerVariable, bool> var_is_registered_;

  // Cache of the literal associated to a [0, 1] variable (positive index).
  // Note that we lazily recompute entries with kNoLiteralIndex since a literal
  // might be associated to the variable later.
  util_intops::StrongVector<IntegerVariable, LiteralIndex> literal_of_var_;

  // If substitution_rep_[var] != kNoIntegerVariable, then we have
  // var == substitution_rep_[var] + substitution_offset_[var].
  util_intops::StrongVector<IntegerVariable, IntegerVariable> substitution_rep_;
  util_intops::StrongVector<IntegerVariable, IntegerValue> substitution_offset_;
  std::vector<std::vector<IntegerVariable>> substituted_vars_by_component_;
  int64_t num_substituted_ = 0;

  int64_t version_ = 0;
  int64_t last_num_redundant_literals_ = 0;
  int64_t last_num_registered_vars_ = 0;

  // Temporary map: (class representative, component) -> (var, sign).
  absl::flat_hash_map<std::pair<BooleanVariable, int>,
                      std::pair<IntegerVariable, bool>>
      tmp_class_to_var_;
};

// This class holds a list of globally valid linear constraints and has some
// logic to decide which ones should be part of the LP relaxation. We want more
// for a better relaxation, but for efficiency we do not want to have too many
// constraints while solving the LP.
//
// This class is meant to contain all the initial constraints of the LP
// relaxation and to get new cuts as they are generated. Thus, it can both
// manage cuts but also only add the initial constraints lazily if there are too
// many of them.
class LinearConstraintManager {
 public:
  struct ConstraintInfo {
    // Note that this constraint always contains "tight" lb/ub, some of these
    // bounds might be trivial level zero bounds, and one can know this by
    // looking at lb_is_trivial/ub_is_trivial.
    LinearConstraint constraint;

    double l2_norm = 0.0;
    double objective_parallelism = 0.0;
    size_t hash;

    // Updated only for deletable constraints. This is incremented every time
    // ChangeLp() is called and the constraint is active in the LP or not in the
    // LP and violated.
    double active_count = 0.0;

    // TODO(user): This is the number of times the constraint was consecutively
    // inactive, and goes up to 100 with the default param, so we could optimize
    // the space used more.
    uint16_t inactive_count = 0;

    // TODO(user): Pack bool and in general optimize the memory of this class.
    bool objective_parallelism_computed = false;
    bool is_in_lp = false;
    bool ub_is_trivial = false;
    bool lb_is_trivial = false;

    // For now, we mark all the generated cuts as deletable and the problem
    // constraints as undeletable.
    // TODO(user): We can have a better heuristic. Some generated good cuts
    // can be marked undeletable and some unused problem specified constraints
    // can be marked deletable.
    bool is_deletable = false;
  };

  explicit LinearConstraintManager(Model* model)
      : sat_parameters_(*model->GetOrCreate<SatParameters>()),
        integer_trail_(*model->GetOrCreate<IntegerTrail>()),
        integer_encoder_(*model->GetOrCreate<IntegerEncoder>()),
        time_limit_(model->GetOrCreate<TimeLimit>()),
        expanded_lp_solution_(*model->GetOrCreate<ModelLpValues>()),
        expanded_reduced_costs_(*model->GetOrCreate<ModelReducedCosts>()),
        model_(model),
        symmetrizer_(model->GetOrCreate<LinearConstraintSymmetrizer>()),
        equivalences_(model->GetOrCreate<BooleanViewEquivalences>()),
        component_(equivalences_->NewComponent()) {}
  ~LinearConstraintManager();

  // Add a new constraint to the manager. Note that we canonicalize constraints
  // and merge the bounds of constraints with the same terms. We also perform
  // basic preprocessing. If added is given, it will be set to true if this
  // constraint was actually a new one and to false if it was dominated by an
  // already existing one.
  DEFINE_STRONG_INDEX_TYPE(ConstraintIndex);
  ConstraintIndex Add(LinearConstraint ct, bool* added = nullptr,
                      bool* folded = nullptr);

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

  // The objective is used as one of the criteria to score cuts.
  // The more a cut is parallel to the objective, the better its score is.
  //
  // Currently this should only be called once per IntegerVariable (Checked). It
  // is easy to support dynamic modification if it becomes needed.
  void SetObjectiveCoefficient(IntegerVariable var, IntegerValue coeff);

  // Heuristic to decide what LP is best solved next. We use the model lp
  // solutions as a heuristic, and it should usually be updated with the last
  // known solution before this call.
  //
  // The current solution state is used for detecting inactive constraints. It
  // is also updated correctly on constraint deletion/addition so that the
  // simplex can be fully iterative on restart by loading this modified state.
  //
  // Returns true iff LpConstraints() will return a different LP than before.
  bool ChangeLp(glop::BasisState* solution_state,
                int* num_new_constraints = nullptr);

  // This can be called initially to add all the current constraints to the LP
  // returned by GetLp().
  void AddAllConstraintsToLp();

  // All the constraints managed by this class.
  const util_intops::StrongVector<ConstraintIndex, ConstraintInfo>&
  AllConstraints() const {
    return constraint_infos_;
  }

  // The set of constraint indices in AllConstraints() that should be part
  // of the next LP to solve.
  const std::vector<ConstraintIndex>& LpConstraints() const {
    return lp_constraints_;
  }

  // For each row of the current LP (see LpConstraints()), the index of the
  // same constraint in the LP before the last call to ChangeLp(), or
  // glop::kInvalidRow if the constraint was added by this last call. This is
  // only valid right after a call to ChangeLp() and is used to transfer
  // information like the dual edge norms between consecutive LPs.
  const std::vector<glop::RowIndex>& OldLpRowOfLpRow() const {
    return old_lp_row_of_lp_row_;
  }

  // To simplify CutGenerator api.
  const util_intops::StrongVector<IntegerVariable, double>& LpValues() {
    return expanded_lp_solution_;
  }
  const util_intops::StrongVector<IntegerVariable, double>& ReducedCosts() {
    return expanded_reduced_costs_;
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
  bool DebugCheckConstraint(const LinearConstraint& cut, bool only_check_ub);

  // Getter "ReducedCosts" API for cuts.
  // One needs to call CacheReducedCostsInfo() before accessing this, otherwise
  // these will just always return zero.
  //
  // It is not possible to set together to true a set of literals 'l' such that
  // sum_l GetLiteralReducedCost(l) > ReducedCostsGap(). Note that we only
  // return non-negative "reduced costs" here.
  void CacheReducedCostsInfo();
  absl::int128 ReducedCostsGap() const { return reduced_costs_gap_; }
  absl::int128 GetLiteralReducedCost(Literal l) const {
    const auto it = reduced_costs_map_.find(l);
    if (it == reduced_costs_map_.end()) return 0;
    return absl::int128(it->second.value());
  }

  // This is quick. Work will happen on CacheReducedCostsInfo().
  // This way if no one uses the information, we don't waste time.
  // See for instance co-1000.mps where this can be slow and we never use
  // the information.
  void SetReducedCostsAsLinearConstraint(const LinearConstraint& ct) {
    reduced_costs_gap_ = 0;
    reduced_costs_map_.clear();
    reduced_costs_is_cached_ = false;
    reduced_cost_constraint_.CopyFrom(ct);
  }

  // Boolean equivalences.
  //
  // During the search, the SAT solver can detect that two Boolean variables are
  // equivalent (see BooleanViewEquivalences). If both have a [0, 1] integer
  // view appearing in this manager, we only want to keep one of them in the
  // LP. We do that by substituting the redundant one in all the constraints
  // (see SimplifyConstraint()).
  //
  // UpdateBooleanEquivalences() updates the shared BooleanViewEquivalences and
  // returns true if the substitution map changed since the last call. This is
  // called by ChangeLp() but can also be called by the client before the LP is
  // created, in which case SimplifyAllConstraints() should be called if it
  // returned true.
  bool UpdateBooleanEquivalences();

  // Runs SimplifyConstraint() on all the constraints. This is automatically
  // done by ChangeLp() when needed.
  void SimplifyAllConstraints();

  // Shortcuts to the BooleanViewEquivalences API for this manager component.
  bool GetSubstitution(IntegerVariable positive_var,
                       AffineExpression* expr) const {
    return equivalences_->GetSubstitution(positive_var, expr);
  }
  const std::vector<IntegerVariable>& SubstitutedVariables() const {
    return equivalences_->SubstitutedVariables(component_);
  }

  int64_t num_substitutions() const { return num_substitutions_; }

 private:
  // Called by Add() and SetObjectiveCoefficient() to keep track of all the
  // variables appearing in this manager.
  void RegisterVariable(IntegerVariable var) {
    equivalences_->RegisterVariable(component_, var);
  }

  // Rewrite the constraint using the current substitution map. Returns true if
  // the constraint changed. The result is in canonical form.
  bool SubstituteEquivalentVariables(LinearConstraint* ct);

  // Must be called after SimplifyConstraint() returned true on the constraint
  // at this index. Recomputes the derived fields and the hash, and merges the
  // constraint with an identical one if there is one.
  void OnConstraintSimplified(ConstraintIndex i);

  // Heuristic that decides which constraints we should remove from the current
  // LP. Note that such constraints can be added back later by the heuristic
  // responsible for adding new constraints from the pool.
  //
  // Returns true if and only if one or more constraints were removed.
  //
  // If the solutions_state is empty, then this function does nothing and
  // returns false (this is used for tests). Otherwise, the solutions_state is
  // assumed to correspond to the current LP and to be of the correct size.
  bool MaybeRemoveSomeInactiveConstraints(glop::BasisState* solution_state);

  // Apply basic inprocessing simplification rules:
  //  - substitute Boolean variables equivalent to another one.
  //  - remove fixed variables
  //  - reduce large coefficients (i.e. coeff strengthening or big-M reduction).
  // This uses level-zero bounds.
  // Returns true if the terms of the constraint changed.
  //
  // If split is not nullptr and the constraint is a ranged constraint for
  // which one of the side would benefit from a stronger coefficient
  // strengthening, the constraint is split in two one-sided constraints: *ct
  // will contain the upper bounded part and *split the lower bounded part.
  // Otherwise *split will be empty (num_terms == 0).
  bool SimplifyConstraint(LinearConstraint* ct,
                          LinearConstraint* split = nullptr);

  // Calls SimplifyConstraint() on the constraint at the given index, and deals
  // with the consequences (rehash, merge, split). If the constraint was split
  // and was in the LP, the other part is added to the LP too and the given
  // solution_state (if not nullptr) is resized accordingly.
  void SimplifyConstraintAtIndex(ConstraintIndex i,
                                 glop::BasisState* solution_state);

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
  const IntegerEncoder& integer_encoder_;

  // Set at true by Add()/SimplifyConstraint() and at false by ChangeLp().
  bool current_lp_is_changed_ = false;

  // Optimization to avoid calling SimplifyConstraint() when not needed.
  int64_t last_simplification_timestamp_ = 0;

  // If the last call to Add() resulted in a split constraint (see
  // SimplifyConstraint()), this is the index of the other part, otherwise this
  // is kInvalidConstraintIndex.
  ConstraintIndex last_split_index_ = ConstraintIndex(-1);

  util_intops::StrongVector<ConstraintIndex, ConstraintInfo> constraint_infos_;

  // The subset of constraints currently in the lp.
  std::vector<ConstraintIndex> lp_constraints_;

  // See OldLpRowOfLpRow().
  std::vector<glop::RowIndex> old_lp_row_of_lp_row_;

  // The constraints that were in the LP and were simplified (or had their
  // bounds tightened by a merge) since the beginning of the last call to
  // ChangeLp(). Such rows cannot reuse the information from the old LP.
  std::vector<ConstraintIndex> simplified_in_lp_constraints_;

  // We keep a map from the hash of our constraint terms to their position in
  // constraints_. This is an optimization to detect duplicate constraints. We
  // are robust to collisions because we always rely on the ground truth
  // contained in constraints_ and the code is still okay if we do not merge the
  // constraints.
  absl::flat_hash_map<size_t, ConstraintIndex> equiv_constraints_;

  // Reduced costs data used by some routing cuts.
  bool reduced_costs_is_cached_ = false;
  absl::int128 reduced_costs_gap_ = 0;
  absl::flat_hash_map<Literal, IntegerValue> reduced_costs_map_;
  LinearConstraint reduced_cost_constraint_;

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

  // Sparse representation of the objective coeffs indexed by positive variable
  // indices. Important: We cannot use a dense representation here in the corner
  // case where we have many independent LPs. Alternatively, we could share a
  // dense vector between all LinearConstraintManager.
  double sum_of_squared_objective_coeffs_ = 0.0;
  absl::flat_hash_map<IntegerVariable, double> objective_map_;

  TimeLimit* time_limit_;
  ModelLpValues& expanded_lp_solution_;
  ModelReducedCosts& expanded_reduced_costs_;
  Model* model_;
  LinearConstraintSymmetrizer* symmetrizer_;

  // Boolean equivalences data. See UpdateBooleanEquivalences().
  BooleanViewEquivalences* equivalences_;
  const int component_;
  int64_t last_equivalences_version_ = 0;
  int64_t num_substitutions_ = 0;
  LinearConstraintBuilder tmp_builder_;

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
// TODO(user): Avoid computing efficacy twice.
// TODO(user): We don't use any orthogonality consideration here.
// TODO(user): Detect duplicate cuts?
class TopNCuts {
 public:
  explicit TopNCuts(int n) : cuts_(n) {}

  // Adds a cut to the local pool.
  void AddCut(
      LinearConstraint ct, absl::string_view name,
      const util_intops::StrongVector<IntegerVariable, double>& lp_solution);

  // Empty the local pool and add all its contents to the manager.
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

#endif  // ORTOOLS_SAT_LINEAR_CONSTRAINT_MANAGER_H_
