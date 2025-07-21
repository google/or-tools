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

#ifndef OR_TOOLS_SAT_CONSTRAINT_VIOLATION_H_
#define OR_TOOLS_SAT_CONSTRAINT_VIOLATION_H_

#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <utility>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "absl/log/check.h"
#include "absl/types/span.h"
#include "ortools/base/stl_util.h"
#include "ortools/sat/cp_model.pb.h"
#include "ortools/sat/sat_parameters.pb.h"
#include "ortools/sat/util.h"
#include "ortools/util/bitset.h"
#include "ortools/util/dense_set.h"
#include "ortools/util/sorted_interval_list.h"
#include "ortools/util/time_limit.h"

namespace operations_research {
namespace sat {

class LinearIncrementalEvaluator {
 public:
  LinearIncrementalEvaluator() = default;

  // Returns the index of the new constraint.
  int NewConstraint(Domain domain);

  // Incrementaly build the constraint.
  //
  // In case of duplicate variables on the same constraint, the code assumes
  // constraints are built in-order as it checks for duplication.
  //
  // Note that the code assume that a Boolean variable DO NOT appear both in the
  // enforcement list and in the constraint. Otherwise the update will just be
  // wrong. This should be enforced by our presolve.
  void AddEnforcementLiteral(int ct_index, int lit);
  void AddLiteral(int ct_index, int lit, int64_t coeff = 1);
  void AddTerm(int ct_index, int var, int64_t coeff, int64_t offset = 0);
  void AddOffset(int ct_index, int64_t offset);
  void AddLinearExpression(int ct_index, const LinearExpressionProto& expr,
                           int64_t multiplier);

  // Important: this needs to be called after all constraint has been added
  // and before the class starts to be used. This is DCHECKed.
  void PrecomputeCompactView(absl::Span<const int64_t> var_max_variation);

  // Compute activities.
  void ComputeInitialActivities(absl::Span<const int64_t> solution);

  // Update the activities of each constraints.
  // Also update the current score for the given deltas.
  //
  // Note that the score of the changed variable will not be updated correctly!
  void UpdateVariableAndScores(
      int var, int64_t delta, absl::Span<const double> weights,
      absl::Span<const int64_t> jump_deltas, absl::Span<double> jump_scores,
      std::vector<int>* constraints_with_changed_violations);

  // Also for feasibility jump.
  void UpdateScoreOnWeightUpdate(int c, absl::Span<const int64_t> jump_deltas,
                                 absl::Span<double> var_to_score_change);

  // Variables whose score/jump might have changed since the last clear.
  //
  // Note that because we reason on a per-constraint basis, this is actually
  // independent of the set of positive constraint weight used.
  void ClearAffectedVariables();
  absl::Span<const int> VariablesAffectedByLastUpdate() const {
    return last_affected_variables_.PositionsSetAtLeastOnce();
  }

  // Query violation.
  int64_t Activity(int c) const;
  int64_t Violation(int c) const;
  bool IsViolated(int c) const;
  bool AppearsInViolatedConstraints(int var) const;

  // Used to DCHECK the state of the evaluator.
  bool VarIsConsistent(int var) const;

  // Intersect constraint bounds with [lb..ub].
  // It returns true if a reduction of the domain took place.
  bool ReduceBounds(int c, int64_t lb, int64_t ub);

  // Model getters.
  int num_constraints() const { return num_constraints_; }

  // Returns the weighted sum of the violation.
  // Note that weights might have more entries than the number of constraints.
  double WeightedViolation(absl::Span<const double> weights) const;

  // Computes how much the weighted violation will change if var was changed
  // from its old value by += delta.
  double WeightedViolationDelta(absl::Span<const double> weights, int var,
                                int64_t delta) const;

  // The violation for each constraint is a piecewise linear function. This
  // computes and aggregates all the breakpoints for the given variable and its
  // domain.
  //
  // Note that if the domain contains less than two values, we return an empty
  // vector. This function is not meant to be used for such domains.
  std::vector<int64_t> SlopeBreakpoints(int var, int64_t current_value,
                                        const Domain& var_domain) const;

  // Checks if the jump value of a variable is always optimal.
  bool ViolationChangeIsConvex(int var) const;

  double DeterministicTime() const {
    return 5e-9 * static_cast<double>(num_ops_);
  }

  int64_t ObjectiveCoefficient(int var) const {
    if (var >= columns_.size()) return 0.0;
    const SpanData& data = columns_[var];
    if (data.num_linear_entries == 0) return 0.0;
    const int i = data.start + data.num_neg_literal + data.num_pos_literal;
    const int c = ct_buffer_[i];
    if (c != 0) return 0.0;
    return coeff_buffer_[data.linear_start];
  }

  absl::Span<const int> ConstraintToVars(int c) const {
    const SpanData& data = rows_[c];
    const int size =
        data.num_pos_literal + data.num_neg_literal + data.num_linear_entries;
    if (size == 0) return {};
    return absl::MakeSpan(&row_var_buffer_[data.start], size);
  }

 private:
  // Cell in the sparse matrix.
  struct Entry {
    int ct_index;
    int64_t coefficient;
  };

  // Column-view of the enforcement literals.
  struct LiteralEntry {
    int ct_index;
    bool positive;  // bool_var or its negation.
  };

  struct SpanData {
    int start = 0;
    int num_pos_literal = 0;
    int num_neg_literal = 0;
    int linear_start = 0;
    int num_linear_entries = 0;
  };

  absl::Span<const int> VarToConstraints(int var) const {
    if (var >= columns_.size()) return {};
    const SpanData& data = columns_[var];
    const int size =
        data.num_pos_literal + data.num_neg_literal + data.num_linear_entries;
    if (size == 0) return {};
    return absl::MakeSpan(&ct_buffer_[data.start], size);
  }

  void ComputeAndCacheDistance(int ct_index);

  // Incremental row-based update.
  void UpdateScoreOnNewlyEnforced(int c, double weight,
                                  absl::Span<const int64_t> jump_deltas,
                                  absl::Span<double> jump_scores);
  void UpdateScoreOnNewlyUnenforced(int c, double weight,
                                    absl::Span<const int64_t> jump_deltas,
                                    absl::Span<double> jump_scores);
  void UpdateScoreOfEnforcementIncrease(int c, double score_change,
                                        absl::Span<const int64_t> jump_deltas,
                                        absl::Span<double> jump_scores);
  void UpdateScoreOnActivityChange(int c, double weight, int64_t activity_delta,
                                   absl::Span<const int64_t> jump_deltas,
                                   absl::Span<double> jump_scores);

  // Constraint indexed data (static).
  int num_constraints_ = 0;
  std::vector<Domain> domains_;
  std::vector<int64_t> offsets_;

  // Variable indexed data.
  // Note that this is just used at construction and is replaced by a compact
  // view when PrecomputeCompactView() is called.
  bool creation_phase_ = true;
  std::vector<std::vector<Entry>> var_entries_;
  std::vector<std::vector<LiteralEntry>> literal_entries_;

  // Memory efficient column based data (static).
  std::vector<SpanData> columns_;
  std::vector<int> ct_buffer_;
  std::vector<int64_t> coeff_buffer_;

  // Memory efficient row based data (static).
  std::vector<SpanData> rows_;
  std::vector<int> row_var_buffer_;
  std::vector<int64_t> row_coeff_buffer_;

  // In order to avoid scanning long constraint we compute for each of them
  // the maximum activity variation of one variable (max-min) * abs(coeff).
  // If the current activity plus this is still feasible, then the constraint
  // do not need to be scanned.
  std::vector<int64_t> row_max_variations_;

  // Temporary data.
  std::vector<int> tmp_row_sizes_;
  std::vector<int> tmp_row_num_positive_literals_;
  std::vector<int> tmp_row_num_negative_literals_;
  std::vector<int> tmp_row_num_linear_entries_;

  // Constraint indexed data (dynamic).
  std::vector<bool> is_violated_;
  std::vector<int64_t> activities_;
  std::vector<int64_t> distances_;
  std::vector<int> num_false_enforcement_;

  // Code to update the scores on a variable change.
  std::vector<int64_t> old_distances_;
  std::vector<int> old_num_false_enforcement_;
  std::vector<int64_t> cached_deltas_;
  std::vector<double> cached_scores_;

  SparseBitset<int> last_affected_variables_;

  mutable size_t num_ops_ = 0;
};

// View of a generic (non linear) constraint for the LsEvaluator.
class CompiledConstraint {
 public:
  CompiledConstraint() = default;
  virtual ~CompiledConstraint() = default;

  // Recomputes the violation of the constraint from scratch.
  void InitializeViolation(absl::Span<const int64_t> solution);

  // Updates the violation with the new value.
  virtual void PerformMove(int var, int64_t old_value,
                           absl::Span<const int64_t> solution_with_new_value);

  // Returns the delta if var changes from old_value to solution[var].
  virtual int64_t ViolationDelta(
      int var, int64_t old_value,
      absl::Span<const int64_t> solution_with_new_value);

  // Returns the sorted vector of variables used by this constraint. This is
  // used to known when a violation might change, and is only called once during
  // initialization, so speed is not to much of a concern here.
  //
  // The global proto is needed to resolve interval variables reference.
  virtual std::vector<int> UsedVariables(
      const CpModelProto& model_proto) const = 0;

  // The cached violation of this constraint.
  int64_t violation() const { return violation_; }

 protected:
  // Computes the violation of a constraint.
  //
  // This is called by InitializeViolation() and also the default implementation
  // of ViolationDelta().
  virtual int64_t ComputeViolation(absl::Span<const int64_t> solution) = 0;

  int64_t violation_;
};

// Intermediate class for all constraints that store directly their proto as
// part of their implementation.
class CompiledConstraintWithProto : public CompiledConstraint {
 public:
  explicit CompiledConstraintWithProto(const ConstraintProto& ct_proto);
  ~CompiledConstraintWithProto() override = default;

  const ConstraintProto& ct_proto() const { return ct_proto_; }

  // This just returns the variables used by the stored ct_proto_.
  std::vector<int> UsedVariables(const CpModelProto& model_proto) const final;

 private:
  const ConstraintProto& ct_proto_;
};

// Evaluation container for the local search.
//
// TODO(user): Ideas for constraint generated moves or sequences of moves?

// Note(user): This class do not handle ALL constraint yet. So it is not because
// there is no violation here that the solution will be feasible. It is
// important to check feasibility once this is used. Note that in practice, we
// can be lucky, and feasible on a subset of hard constraint is enough.
class LsEvaluator {
 public:
  // The cp_model must outlive this class.
  LsEvaluator(const CpModelProto& cp_model, const SatParameters& params,
              TimeLimit* time_limit);
  LsEvaluator(const CpModelProto& cp_model, const SatParameters& params,
              const std::vector<bool>& ignored_constraints,
              absl::Span<const ConstraintProto> additional_constraints,
              TimeLimit* time_limit);

  // Intersects the domain of the objective with [lb..ub].
  // It returns true if a reduction of the domain took place.
  bool ReduceObjectiveBounds(int64_t lb, int64_t ub);

  // Recomputes the violations of all constraints (resp only non-linear one).
  void ComputeAllViolations(absl::Span<const int64_t> solution);
  void ComputeAllNonLinearViolations(absl::Span<const int64_t> solution);

  // Recomputes the violations of all impacted non linear constraints.
  void UpdateNonLinearViolations(int var, int64_t old_value,
                                 absl::Span<const int64_t> new_solution);

  // Function specific to the linear only feasibility jump.
  void UpdateLinearScores(int var, int64_t old_value, int64_t new_value,
                          absl::Span<const double> weights,
                          absl::Span<const int64_t> jump_deltas,
                          absl::Span<double> jump_scores);

  // Must be called after UpdateLinearScores() / UpdateNonLinearViolations()
  // in order to update the ViolatedConstraints().
  void UpdateViolatedList();

  absl::Span<const int> VariablesAffectedByLastLinearUpdate() const {
    return linear_evaluator_.VariablesAffectedByLastUpdate();
  }

  // Simple summation metric for the constraint and objective violations.
  int64_t SumOfViolations();

  // Returns the objective activity in the current state.
  int64_t ObjectiveActivity() const;

  bool IsObjectiveConstraint(int c) const {
    return cp_model_.has_objective() && c == 0;
  }

  int64_t ObjectiveCoefficient(int var) const {
    return cp_model_.has_objective()
               ? linear_evaluator_.ObjectiveCoefficient(var)
               : 0;
  }

  // The number of "internal" constraints in the evaluator. This might not
  // be the same as the number of initial constraints of the model.
  int NumLinearConstraints() const;
  int NumNonLinearConstraints() const;
  int NumEvaluatorConstraints() const;
  int NumInfeasibleConstraints() const;

  // Returns the weighted sum of violation. The weights must have the same
  // size as NumEvaluatorConstraints().
  int64_t Violation(int c) const;
  bool IsViolated(int c) const;
  double WeightedViolation(absl::Span<const double> weights) const;

  // Computes the delta in weighted violation if solution[var] += delta.
  // We need a temporary mutable solution to evaluate the violation of generic
  // constraints. If linear_only is true, only the linear violation will be
  // used.
  double WeightedViolationDelta(bool linear_only,
                                absl::Span<const double> weights, int var,
                                int64_t delta,
                                absl::Span<int64_t> mutable_solution) const;

  const LinearIncrementalEvaluator& LinearEvaluator() {
    return linear_evaluator_;
  }

  LinearIncrementalEvaluator* MutableLinearEvaluator() {
    return &linear_evaluator_;
  }

  // List of the currently violated constraints.
  // - It is initialized by RecomputeViolatedList()
  // - And incrementally maintained by UpdateVariableValue()
  //
  // The order depends on the algorithm used and shouldn't be relied on.
  void RecomputeViolatedList(bool linear_only);
  absl::Span<const int> ViolatedConstraints() const {
    return violated_constraints_.values();
  }

  // Returns the number of constraints in ViolatedConstraints containing `var`.
  int NumViolatedConstraintsForVarIgnoringObjective(int var) const {
    return num_violated_constraint_per_var_ignoring_objective_[var];
  }

  // Indicates if the computed jump value is always the best choice.
  bool VariableOnlyInLinearConstraintWithConvexViolationChange(int var) const;

  const std::vector<int>& last_update_violation_changes() const {
    return last_update_violation_changes_;
  }

  absl::Span<const int> ConstraintToVars(int c) const {
    if (c < NumLinearConstraints()) {
      return linear_evaluator_.ConstraintToVars(c);
    }
    return absl::MakeConstSpan(constraint_to_vars_[c - NumLinearConstraints()]);
  }

  // Note that the constraint indexing is different here than in the other
  // functions.
  absl::Span<const int> VarToGeneralConstraints(int var) const {
    return var_to_constraints_[var];
  }
  absl::Span<const int> GeneralConstraintToVars(int general_c) const {
    return constraint_to_vars_[general_c];
  }

  // TODO(user): Properly account all big time consumers.
  double DeterministicTime() const {
    return linear_evaluator_.DeterministicTime() + dtime_;
  }

 private:
  void CompileConstraintsAndObjective(
      const std::vector<bool>& ignored_constraints,
      absl::Span<const ConstraintProto> additional_constraints);

  void CompileOneConstraint(const ConstraintProto& ct_proto);
  void BuildVarConstraintGraph();
  void UpdateViolatedList(int c);

  const CpModelProto& cp_model_;
  const SatParameters& params_;
  CpModelProto expanded_constraints_;
  LinearIncrementalEvaluator linear_evaluator_;
  std::vector<std::unique_ptr<CompiledConstraint>> constraints_;
  std::vector<std::vector<int>> var_to_constraints_;
  std::vector<double> var_to_dtime_estimate_;
  std::vector<std::vector<int>> constraint_to_vars_;
  std::vector<bool> jump_value_optimal_;
  TimeLimit* time_limit_;

  UnsafeDenseSet<int> violated_constraints_;
  std::vector<int> num_violated_constraint_per_var_ignoring_objective_;

  // Constraint index with changed violations.
  std::vector<int> last_update_violation_changes_;

  mutable double dtime_ = 0;
};

// ================================
// Individual compiled constraints.
// ================================

// The violation of a bool_xor constraint is 0 or 1.
class CompiledBoolXorConstraint : public CompiledConstraintWithProto {
 public:
  explicit CompiledBoolXorConstraint(const ConstraintProto& ct_proto);
  ~CompiledBoolXorConstraint() override = default;

  int64_t ComputeViolation(absl::Span<const int64_t> solution) override;
  int64_t ViolationDelta(
      int /*var*/, int64_t /*old_value*/,
      absl::Span<const int64_t> solution_with_new_value) override;
};

// The violation of a lin_max constraint is:
// - the sum(max(0, expr_value - target_value) forall expr). This part will be
//   maintained by the linear part.
// - target_value - max(expressions) if positive.
class CompiledLinMaxConstraint : public CompiledConstraintWithProto {
 public:
  explicit CompiledLinMaxConstraint(const ConstraintProto& ct_proto);
  ~CompiledLinMaxConstraint() override = default;

  int64_t ComputeViolation(absl::Span<const int64_t> solution) override;
};

// The violation of an int_prod constraint is
//     abs(value(target) - prod(value(expr)).
class CompiledIntProdConstraint : public CompiledConstraintWithProto {
 public:
  explicit CompiledIntProdConstraint(const ConstraintProto& ct_proto);
  ~CompiledIntProdConstraint() override = default;

  int64_t ComputeViolation(absl::Span<const int64_t> solution) override;
};

// The violation of an int_div constraint is
//     abs(value(target) - value(expr0) / value(expr1)).
class CompiledIntDivConstraint : public CompiledConstraintWithProto {
 public:
  explicit CompiledIntDivConstraint(const ConstraintProto& ct_proto);
  ~CompiledIntDivConstraint() override = default;

  int64_t ComputeViolation(absl::Span<const int64_t> solution) override;
};

// The violation of an int_mod constraint is defined as follow:
//
// if target and expr0 have the same sign:
//   min(
//     abs(value(target) - (value(expr0) % value(expr1))),
//     abs(value(target)) + abs((value(expr0) % value(expr1)) - value(expr1)),
//     abs(value(expr0) % value(expr1)) + abs(value(target) - value(expr1)),
//   )
//
// if target and expr0 have different sign:
//   abs(target) + abs(expr0)
// Note: the modulo (expr1) is always fixed.
class CompiledIntModConstraint : public CompiledConstraintWithProto {
 public:
  explicit CompiledIntModConstraint(const ConstraintProto& ct_proto);
  ~CompiledIntModConstraint() override = default;

  int64_t ComputeViolation(absl::Span<const int64_t> solution) override;
};

// The violation of a all_diff is the number of unordered pairs of expressions
// with the same value.
class CompiledAllDiffConstraint : public CompiledConstraintWithProto {
 public:
  explicit CompiledAllDiffConstraint(const ConstraintProto& ct_proto);
  ~CompiledAllDiffConstraint() override = default;

  int64_t ComputeViolation(absl::Span<const int64_t> solution) override;

 private:
  std::vector<int64_t> values_;
};

// This is more compact and faster to destroy than storing a
// LinearExpressionProto.
struct ViewOfAffineLinearExpressionProto {
  explicit ViewOfAffineLinearExpressionProto(
      const LinearExpressionProto& proto) {
    if (!proto.vars().empty()) {
      DCHECK_EQ(proto.vars().size(), 1);
      var = proto.vars(0);
      coeff = proto.coeffs(0);
    }
    offset = proto.offset();
  }

  void AppendVarTo(std::vector<int>& result) const {
    if (coeff != 0) result.push_back(var);
  }

  int var = 0;
  int64_t coeff = 0;
  int64_t offset = 0;
};

// Special constraint for no overlap between two intervals.
// We usually expand small no-overlap in n^2 such constraint, so we want to
// be compact and efficient here.
template <bool has_enforcement = true>
class CompiledNoOverlapWithTwoIntervals : public CompiledConstraint {
 public:
  struct Interval {
    explicit Interval(const ConstraintProto& x)
        : start(x.interval().start()), end(x.interval().end()) {}
    ViewOfAffineLinearExpressionProto start;
    ViewOfAffineLinearExpressionProto end;
  };

  CompiledNoOverlapWithTwoIntervals(const ConstraintProto& x1,
                                    const ConstraintProto& x2)
      : interval1_(x1), interval2_(x2) {
    if (has_enforcement) {
      enforcements_.assign(x1.enforcement_literal().begin(),
                           x1.enforcement_literal().end());
      enforcements_.insert(enforcements_.end(),
                           x2.enforcement_literal().begin(),
                           x2.enforcement_literal().end());
      gtl::STLSortAndRemoveDuplicates(&enforcements_);
    }
  }

  ~CompiledNoOverlapWithTwoIntervals() final = default;

  int64_t ComputeViolation(absl::Span<const int64_t> solution) final {
    // Optimization hack: If we create a ComputeViolationInternal() that we call
    // from here and in ViolationDelta(), then the later is not inlined below in
    // ViolationDelta() where it matter a lot for performance.
    violation_ = 0;
    violation_ = ViolationDelta(0, 0, solution);
    return violation_;
  }

  int64_t ViolationDelta(
      int /*var*/, int64_t /*old_value*/,
      absl::Span<const int64_t> solution_with_new_value) final;

  std::vector<int> UsedVariables(const CpModelProto& model_proto) const final;

 private:
  std::vector<int> enforcements_;
  const Interval interval1_;
  const Interval interval2_;
};

class CompiledNoOverlap2dConstraint : public CompiledConstraintWithProto {
 public:
  CompiledNoOverlap2dConstraint(const ConstraintProto& ct_proto,
                                const CpModelProto& cp_model);
  ~CompiledNoOverlap2dConstraint() override = default;

  int64_t ComputeViolation(absl::Span<const int64_t> solution) override;

 private:
  const CpModelProto& cp_model_;
};

template <bool has_enforcement = true>
class CompiledNoOverlap2dWithTwoBoxes : public CompiledConstraint {
 public:
  struct Box {
    Box(const ConstraintProto& x, const ConstraintProto& y)
        : x_min(x.interval().start()),
          x_max(x.interval().end()),
          y_min(y.interval().start()),
          y_max(y.interval().end()) {}
    ViewOfAffineLinearExpressionProto x_min;
    ViewOfAffineLinearExpressionProto x_max;
    ViewOfAffineLinearExpressionProto y_min;
    ViewOfAffineLinearExpressionProto y_max;
  };

  CompiledNoOverlap2dWithTwoBoxes(const ConstraintProto& x1,
                                  const ConstraintProto& y1,
                                  const ConstraintProto& x2,
                                  const ConstraintProto& y2)
      : box1_(x1, y1), box2_(x2, y2) {
    if (has_enforcement) {
      enforcements_.assign(x1.enforcement_literal().begin(),
                           x1.enforcement_literal().end());
      enforcements_.insert(enforcements_.end(),
                           y1.enforcement_literal().begin(),
                           y1.enforcement_literal().end());
      enforcements_.insert(enforcements_.end(),
                           x2.enforcement_literal().begin(),
                           x2.enforcement_literal().end());
      enforcements_.insert(enforcements_.end(),
                           y2.enforcement_literal().begin(),
                           y2.enforcement_literal().end());
      gtl::STLSortAndRemoveDuplicates(&enforcements_);
    }
  }

  ~CompiledNoOverlap2dWithTwoBoxes() final = default;

  int64_t ComputeViolation(absl::Span<const int64_t> solution) final {
    // Optimization hack: If we create a ComputeViolationInternal() that we call
    // from here and in ViolationDelta(), then the later is not inlined below in
    // ViolationDelta() where it matter a lot for performance.
    violation_ = 0;
    violation_ = ViolationDelta(0, 0, solution);
    return violation_;
  }

  // Note(user): this is the same implementation as the base one, but it
  // avoid one virtual call !
  int64_t ViolationDelta(
      int /*var*/, int64_t /*old_value*/,
      absl::Span<const int64_t> solution_with_new_value) final;

  std::vector<int> UsedVariables(const CpModelProto& model_proto) const final;

 private:
  std::vector<int> enforcements_;
  const Box box1_;
  const Box box2_;
};

// This can be used to encode reservoir or a cumulative constraints for LS. We
// have a set of event time, and we use for overall violation the sum of
// overload over time.
//
// This version support an incremental computation when just a few events
// changes, which is roughly O(n) instead of O(n log n) which makes it
// significantly faster than recomputing and sorting the profile on each
// ViolationDelta().
class CompiledReservoirConstraint : public CompiledConstraint {
 public:
  CompiledReservoirConstraint(LinearExpressionProto capacity,
                              std::vector<std::optional<int>> is_active,
                              std::vector<LinearExpressionProto> times,
                              std::vector<LinearExpressionProto> demands)
      : capacity_(std::move(capacity)),
        is_active_(std::move(is_active)),
        times_(std::move(times)),
        demands_(std::move(demands)) {
    const int num_events = times_.size();
    time_values_.resize(num_events, 0);
    demand_values_.resize(num_events, 0);
    InitializeDenseIndexToEvents();
  }

  // Note that since we have our own ViolationDelta() implementation this is
  // only used for initialization and our PerformMove(). It is why we set
  // violations_ here.
  int64_t ComputeViolation(absl::Span<const int64_t> solution) final {
    violation_ = BuildProfileAndReturnViolation(solution);
    return violation_;
  }

  void PerformMove(int /*var*/, int64_t /*old_value*/,
                   absl::Span<const int64_t> solution_with_new_value) final {
    // TODO(user): we could probably be more incremental here, but it is a bit
    // tricky to get right and not too important since the time is dominated by
    // evaluating moves, not taking them.
    ComputeViolation(solution_with_new_value);
  }

  int64_t ViolationDelta(
      int var, int64_t /*old_value*/,
      absl::Span<const int64_t> solution_with_new_value) final {
    return IncrementalViolation(var, solution_with_new_value) - violation_;
  }

  std::vector<int> UsedVariables(const CpModelProto& model_proto) const final;

 private:
  // This works in O(n log n).
  int64_t BuildProfileAndReturnViolation(absl::Span<const int64_t> solution);

  // This works in O(n) + O(d log d) where d is the number of modified events
  // compare to the base solution. In most situation it should be O(1).
  int64_t IncrementalViolation(int var, absl::Span<const int64_t> solution);

  // This is used to speed up IncrementalViolation().
  void InitializeDenseIndexToEvents();
  void AppendVariablesForEvent(int i, std::vector<int>* result) const;

  // The const data from the constructor.
  // Note that is_active_ might be empty if all events are mandatory.
  const LinearExpressionProto capacity_;
  const std::vector<std::optional<int>> is_active_;
  const std::vector<LinearExpressionProto> times_;
  const std::vector<LinearExpressionProto> demands_;

  // Remap all UsedVariables() to a dense index in [0, num_used_vars).
  absl::flat_hash_map<int, int> var_to_dense_index_;
  CompactVectorVector<int, int> dense_index_to_events_;

  struct Event {
    int64_t time;
    int64_t demand;
    bool operator<(const Event& o) const { return time < o.time; }
  };
  std::vector<Event> profile_;
  std::vector<Event> profile_delta_;

  // This is filled by BuildProfileAndReturnViolation() and correspond to the
  // value in the current solutions.
  int64_t capacity_value_;
  std::vector<int64_t> time_values_;
  std::vector<int64_t> demand_values_;
};

}  // namespace sat
}  // namespace operations_research

#endif  // OR_TOOLS_SAT_CONSTRAINT_VIOLATION_H_
