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

#ifndef OR_TOOLS_SAT_PRECEDENCES_H_
#define OR_TOOLS_SAT_PRECEDENCES_H_

#include <algorithm>
#include <cstdint>
#include <tuple>
#include <utility>
#include <variant>
#include <vector>

#include "absl/container/btree_set.h"
#include "absl/container/flat_hash_map.h"
#include "absl/container/flat_hash_set.h"
#include "absl/container/inlined_vector.h"
#include "absl/log/check.h"
#include "absl/strings/str_format.h"
#include "absl/types/span.h"
#include "ortools/base/strong_vector.h"
#include "ortools/sat/cp_model_mapping.h"
#include "ortools/sat/implied_bounds.h"
#include "ortools/sat/integer.h"
#include "ortools/sat/integer_base.h"
#include "ortools/sat/model.h"
#include "ortools/sat/sat_base.h"
#include "ortools/sat/synchronization.h"
#include "ortools/sat/util.h"
#include "ortools/util/rev.h"
#include "ortools/util/strong_integers.h"

namespace operations_research {
namespace sat {

DEFINE_STRONG_INDEX_TYPE(LinearExpression2Index);
const LinearExpression2Index kNoLinearExpression2Index(-1);
inline LinearExpression2Index NegationOf(LinearExpression2Index i) {
  return LinearExpression2Index(i.value() ^ 1);
}

inline bool Linear2IsPositive(LinearExpression2Index i) {
  return (i.value() & 1) == 0;
}

inline LinearExpression2Index PositiveLinear2(LinearExpression2Index i) {
  return LinearExpression2Index(i.value() & (~1));
}

// Class to hold a list of LinearExpression2 that have (potentially) non-trivial
// bounds. This class is overzealous, in the sense that if a linear2 is in the
// list, it does not necessarily mean that it has a non-trivial bound, but the
// converse is true: if a linear2 is not in the list,
// Linear2Bounds::GetUpperBound() will return a trivial bound.
class Linear2Indices {
 public:
  Linear2Indices() = default;

  // Returns a never-changing index for the given linear expression.
  // The expression must already be canonicalized and divided by its GCD.
  LinearExpression2Index AddOrGet(LinearExpression2 expr);

  // Returns a never-changing index for the given linear expression if it is
  // potentially non-trivial, otherwise returns kNoLinearExpression2Index. The
  // expression must already be canonicalized and divided by its GCD.
  LinearExpression2Index GetIndex(LinearExpression2 expr) const;

  LinearExpression2 GetExpression(LinearExpression2Index index) const;

  // Return all positive linear2 expressions that have a potentially non-trivial
  // bound. When calling this code it is often a good idea to check both the
  // expression on the span and its negation. The order is fixed forever and
  // this span can only grow by appending new expressions.
  absl::Span<const LinearExpression2> GetStoredLinear2Indices() const {
    return exprs_;
  }

  // Return a list of all potentially non-trivial LinearExpression2Indexes
  // containing a given variable.
  absl::Span<const LinearExpression2Index> GetAllLinear2ContainingVariable(
      IntegerVariable var) const;

  // Return a list of all potentially non-trivial LinearExpression2Indexes
  // containing a given pair of variables.
  absl::Span<const LinearExpression2Index> GetAllLinear2ContainingVariables(
      IntegerVariable var1, IntegerVariable var2) const;

 private:
  std::vector<LinearExpression2> exprs_;
  absl::flat_hash_map<LinearExpression2, int> expr_to_index_;

  // Map to implement GetAllBoundsContainingVariable().
  absl::flat_hash_map<IntegerVariable,
                      absl::InlinedVector<LinearExpression2Index, 2>>
      var_to_bounds_;
  // Map to implement GetAllBoundsContainingVariables().
  absl::flat_hash_map<std::pair<IntegerVariable, IntegerVariable>,
                      absl::InlinedVector<LinearExpression2Index, 1>>
      var_pair_to_bounds_;
};

// Simple "watcher" class that will be notified if a linear2 bound changed. It
// can also be queried to see if LinearExpression2 involving a specific variable
// changed since last time.
class Linear2Watcher {
 public:
  explicit Linear2Watcher(Model* model)
      : watcher_(model->GetOrCreate<GenericLiteralWatcher>()) {}

  // This assumes `expr` is canonicalized and divided by its gcd.
  void NotifyBoundChanged(LinearExpression2 expr);

  // Register a GenericLiteralWatcher() id so that propagation is called as
  // soon as a bound on a linear2 changed.
  void WatchAllLinearExpressions2(int id) { propagator_ids_.insert(id); }

  // Allow to know if some bounds changed since last query.
  int64_t Timestamp() const { return timestamp_; }
  int64_t VarTimestamp(IntegerVariable var);

 private:
  GenericLiteralWatcher* watcher_;

  int64_t timestamp_ = 0;
  util_intops::StrongVector<IntegerVariable, int64_t> var_timestamp_;
  absl::btree_set<int> propagator_ids_;
};

// This holds all the relation lhs <= linear2 <= rhs that are true at level
// zero. It is the source of truth across all the solver for such bounds.
class RootLevelLinear2Bounds {
 public:
  explicit RootLevelLinear2Bounds(Model* model)
      : integer_trail_(model->GetOrCreate<IntegerTrail>()),
        linear2_watcher_(model->GetOrCreate<Linear2Watcher>()),
        shared_stats_(model->GetOrCreate<SharedStatistics>()),
        lin2_indices_(model->GetOrCreate<Linear2Indices>()),
        cp_model_mapping_(model->GetOrCreate<CpModelMapping>()),
        shared_linear2_bounds_(model->Mutable<SharedLinear2Bounds>()),
        shared_linear2_bounds_id_(
            shared_linear2_bounds_ == nullptr
                ? 0
                : shared_linear2_bounds_->RegisterNewId(model->Name())) {}

  ~RootLevelLinear2Bounds();

  // Add a relation lb <= expr <= ub. If expr is not a proper linear2 expression
  // (e.g. 0*x + y, y + y, y - y) it will be ignored.
  // Returns a pair saying whether the lower/upper bounds for this expr became
  // more restricted than what was currently stored.
  std::pair<bool, bool> Add(LinearExpression2 expr, IntegerValue lb,
                            IntegerValue ub) {
    if (integer_trail_->LevelZeroUpperBound(expr) <= ub &&
        integer_trail_->LevelZeroLowerBound(expr) >= lb) {
      return {false, false};
    }
    const bool negated = expr.CanonicalizeAndUpdateBounds(lb, ub);
    if (expr.coeffs[0] == 0 || expr.coeffs[1] == 0) return {false, false};
    const LinearExpression2Index index = lin2_indices_->AddOrGet(expr);
    bool ub_changed = AddUpperBound(index, ub);
    bool lb_changed = AddUpperBound(NegationOf(index), -lb);
    if (negated) {
      std::swap(lb_changed, ub_changed);
    }
    return {lb_changed, ub_changed};
  }

  // Same as above, but only update the upper bound.
  bool AddUpperBound(LinearExpression2 expr, IntegerValue ub) {
    if (integer_trail_->LevelZeroUpperBound(expr) <= ub) return false;
    expr.SimpleCanonicalization();
    if (expr.coeffs[0] == 0 || expr.coeffs[1] == 0) return false;
    const IntegerValue gcd = expr.DivideByGcd();
    ub = FloorRatio(ub, gcd);
    return AddUpperBound(lin2_indices_->AddOrGet(expr), ub);
  }

  bool AddLowerBound(LinearExpression2 expr, IntegerValue lb) {
    expr.Negate();
    return AddUpperBound(expr, -lb);
  }

  bool AddLowerBound(LinearExpression2Index index, IntegerValue lb) {
    return AddUpperBound(NegationOf(index), -lb);
  }

  // All modifications go through this function.
  bool AddUpperBound(LinearExpression2Index index, IntegerValue ub);

  IntegerValue LevelZeroUpperBound(LinearExpression2 expr) const {
    expr.SimpleCanonicalization();
    if (expr.coeffs[0] == 0 || expr.coeffs[1] == 0) {
      return integer_trail_->LevelZeroUpperBound(expr);
    }
    const IntegerValue gcd = expr.DivideByGcd();
    const LinearExpression2Index index = lin2_indices_->GetIndex(expr);
    if (index == kNoLinearExpression2Index) {
      return integer_trail_->LevelZeroUpperBound(expr);
    }
    return CapProdI(gcd, LevelZeroUpperBound(index));
  }

  IntegerValue LevelZeroUpperBound(LinearExpression2Index index) const {
    const LinearExpression2 expr = lin2_indices_->GetExpression(index);
    // TODO(user): Remove the expression from the root_level_relations_ if
    // the zero-level bound got more restrictive.
    return std::min(integer_trail_->LevelZeroUpperBound(expr),
                    GetUpperBoundNoTrail(index));
  }

  // Return a list of (expr <= ub) sorted by expr. They are guaranteed to be
  // better than the trivial upper bound.
  std::vector<std::pair<LinearExpression2, IntegerValue>>
  GetSortedNonTrivialUpperBounds() const;

  // Return a list of (lb <= expr <= ub), with expr.vars[0] = var, where at
  // least one of the bounds is non-trivial and the potential other non-trivial
  // bound is tight.
  //
  // As the class name indicates, all bounds are level zero ones.
  std::vector<std::tuple<LinearExpression2, IntegerValue, IntegerValue>>
  GetAllBoundsContainingVariable(IntegerVariable var) const;

  // Return a list of (lb <= expr <= ub), with expr.vars = {var1, var2}, where
  // at least one of the bounds is non-trivial and the potential other
  // non-trivial bound is tight.
  //
  // As the class name indicates, all bounds are level zero ones.
  std::vector<std::tuple<LinearExpression2, IntegerValue, IntegerValue>>
  GetAllBoundsContainingVariables(IntegerVariable var1,
                                  IntegerVariable var2) const;

  // For a given variable `var`, return all variables `other` so that
  // LinearExpression2(var, other, 1, 1) has a non trivial upper bound.
  // Note that using negation one can also recover x + y >= lb and x - y <= ub.
  absl::Span<const std::pair<IntegerVariable, LinearExpression2Index>>
  GetVariablesInSimpleRelation(IntegerVariable var) const;

  // For all pairs of relation 'a + var <= x' and 'neg(var) + b <= y' try to add
  // 'a + b <= x + y' if that relation is better.
  //
  // This can be quadratic. Returns the amount of "work" done, and abort if
  // we reach the limit. This uses GetVariablesInSimpleRelation().
  int AugmentSimpleRelations(IntegerVariable var, int work_limit);

  RelationStatus GetLevelZeroStatus(LinearExpression2 expr, IntegerValue lb,
                                    IntegerValue ub) const;

  // Low-level function that returns the zero-level upper bound if it is
  // non-trivial. Otherwise returns kMaxIntegerValue. This is a different
  // behavior from LevelZeroUpperBound() that would return the implied
  // zero-level bound from the trail for trivial ones. `expr` must be
  // canonicalized and gcd-reduced.
  IntegerValue GetUpperBoundNoTrail(LinearExpression2Index index) const;

  int64_t num_updates() const { return num_updates_; }

 private:
  IntegerTrail* integer_trail_;
  Linear2Watcher* linear2_watcher_;
  SharedStatistics* shared_stats_;
  Linear2Indices* lin2_indices_;
  CpModelMapping* cp_model_mapping_;
  SharedLinear2Bounds* shared_linear2_bounds_;  // Might be nullptr.

  const int shared_linear2_bounds_id_;

  util_intops::StrongVector<LinearExpression2Index, IntegerValue>
      best_upper_bounds_;

  // coeff_one_var_lookup_[var] contains all the other_var such that we have a
  // linear2 relation var + other_var <= ub. We also store that relation index.
  util_intops::StrongVector<LinearExpression2Index, bool> in_coeff_one_lookup_;
  util_intops::StrongVector<
      IntegerVariable,
      std::vector<std::pair<IntegerVariable, LinearExpression2Index>>>
      coeff_one_var_lookup_;

  int64_t num_updates_ = 0;
};

struct FullIntegerPrecedence {
  IntegerVariable var;
  std::vector<int> indices;
  std::vector<IntegerValue> offsets;
};

// This class is used to compute the transitive closure of the level-zero
// precedence relations.
//
// TODO(user): Support non-DAG like graph.
class TransitivePrecedencesEvaluator {
 public:
  explicit TransitivePrecedencesEvaluator(Model* model)
      : params_(model->GetOrCreate<SatParameters>()),
        integer_trail_(model->GetOrCreate<IntegerTrail>()),
        shared_stats_(model->GetOrCreate<SharedStatistics>()),
        root_level_bounds_(model->GetOrCreate<RootLevelLinear2Bounds>()) {
    // Call Build() each time we go back to level zero.
    model->GetOrCreate<LevelZeroCallbackHelper>()->callbacks.push_back(
        [this]() { return Build(); });
  }

  // Returns a set of relations var >= max_i(vars[index[i]] + offsets[i]).
  //
  // This currently only works if the precedence relation form a DAG.
  // If not we will just abort. TODO(user): generalize.
  //
  // For more efficiency, this method ignores all linear2 expressions with any
  // coefficient different from 1.
  //
  // TODO(user): Put some work limit in place, as this can be slow. Complexity
  // is in O(vars.size()) * num_arcs.
  //
  // TODO(user): Since we don't need ALL precedences, we could just work on a
  // sub-DAG of the full precedence graph instead of aborting. Or we can just
  // support the general non-DAG cases.
  //
  // TODO(user): Many relations can be redundant. Filter them.
  void ComputeFullPrecedences(absl::Span<const IntegerVariable> vars,
                              std::vector<FullIntegerPrecedence>* output);

  // The current code requires the internal data to be processed once all
  // root-level relations are loaded.
  //
  // If we don't have too many variable, we compute the full transitive closure
  // and then push back to RootLevelLinear2Bounds if there is a relation between
  // two variables. This can be used to optimize some scheduling propagation and
  // reasons.
  //
  // Warning: If there are too many, this will NOT push all relations.
  bool Build();

 private:
  SatParameters* params_;
  IntegerTrail* integer_trail_;
  SharedStatistics* shared_stats_;
  RootLevelLinear2Bounds* root_level_bounds_;

  int64_t build_timestamp_ = -1;
  bool is_dag_ = false;
  std::vector<IntegerVariable> topological_order_;
};

// Stores all the precedences relation of the form "{lits} => a*x + b*y <= ub"
// that we could extract from the model.
class EnforcedLinear2Bounds : public ReversibleInterface {
 public:
  explicit EnforcedLinear2Bounds(Model* model)
      : params_(*model->GetOrCreate<SatParameters>()),
        trail_(model->GetOrCreate<Trail>()),
        integer_trail_(model->GetOrCreate<IntegerTrail>()),
        linear2_watcher_(model->GetOrCreate<Linear2Watcher>()),
        root_level_bounds_(model->GetOrCreate<RootLevelLinear2Bounds>()),
        shared_stats_(model->GetOrCreate<SharedStatistics>()),
        lin2_indices_(model->GetOrCreate<Linear2Indices>()) {
    integer_trail_->RegisterReversibleClass(this);
  }

  ~EnforcedLinear2Bounds() override;

  // Adds add relation (enf => expr <= rhs) that is assumed to be true at
  // the current level.
  //
  // It will be automatically reverted via the SetLevel() functions that is
  // called before any integer propagations trigger.
  //
  // This is assumed to be called when a relation becomes true (enforcement are
  // assigned) and when it becomes false in reverse order (CHECKed).
  //
  // If expr is not a proper linear2 expression (e.g. 0*x + y, y + y, y - y) it
  // will be ignored.
  void PushConditionalRelation(absl::Span<const Literal> enforcements,
                               LinearExpression2Index index, IntegerValue rhs);

  void PushConditionalRelation(absl::Span<const Literal> enforcements,
                               LinearExpression2 expr, IntegerValue rhs) {
    expr.SimpleCanonicalization();
    if (expr.coeffs[0] == 0 || expr.coeffs[1] == 0) return;
    const IntegerValue gcd = expr.DivideByGcd();
    rhs = FloorRatio(rhs, gcd);
    return PushConditionalRelation(enforcements, lin2_indices_->AddOrGet(expr),
                                   rhs);
  }

  // Called each time we change decision level.
  void SetLevel(int level) final;

  void SetLevelToTrail() {
    if (trail_->CurrentDecisionLevel() != stored_level_) {
      SetLevel(trail_->CurrentDecisionLevel());
    }
  }

  // Returns a set of precedences such that we have a relation
  // of the form vars[index] <= var + offset.
  //
  // All entries for the same variable will be contiguous and sorted by index.
  // We only list variable with at least two entries. The up to date offset can
  // be retrieved later via Linear2Bounds::UpperBound(lin2_index).
  //
  // This method currently ignores all linear2 expressions with any coefficient
  // different from 1.
  //
  // TODO(user): Ideally this should be moved to a new class and maybe augmented
  // with other kind of precedences.
  struct PrecedenceData {
    IntegerVariable var;
    int var_index;
    LinearExpression2Index lin2_index;
  };
  void CollectPrecedences(absl::Span<const IntegerVariable> vars,
                          std::vector<PrecedenceData>* output);

  // Low-level function that returns the upper bound if there is some enforced
  // relations only. Otherwise always returns kMaxIntegerValue.
  // `expr` must be canonicalized and gcd-reduced.
  IntegerValue GetUpperBoundFromEnforced(LinearExpression2Index index) const;

  void AddReasonForUpperBoundLowerThan(
      LinearExpression2Index index, IntegerValue ub,
      std::vector<Literal>* literal_reason,
      std::vector<IntegerLiteral>* integer_reason) const;

 private:
  void CreateLevelEntryIfNeeded();

  const SatParameters& params_;
  Trail* trail_;
  IntegerTrail* integer_trail_;
  Linear2Watcher* linear2_watcher_;
  RootLevelLinear2Bounds* root_level_bounds_;
  SharedStatistics* shared_stats_;
  Linear2Indices* lin2_indices_;

  int64_t num_conditional_relation_updates_ = 0;
  int stored_level_ = 0;

  // Conditional stack for push/pop of conditional relations.
  //
  // TODO(user): this kind of reversible hash_map is already implemented in
  // other part of the code. Consolidate.
  struct ConditionalEntry {
    ConditionalEntry(int p, IntegerValue r, LinearExpression2Index k,
                     absl::Span<const Literal> e)
        : prev_entry(p), rhs(r), key(k), enforcements(e.begin(), e.end()) {}

    int prev_entry;
    IntegerValue rhs;
    LinearExpression2Index key;
    absl::InlinedVector<Literal, 4> enforcements;
  };
  std::vector<ConditionalEntry> conditional_stack_;
  std::vector<std::pair<int, int>> level_to_stack_size_;

  // This is always stored in the form (expr <= rhs).
  // The conditional relations contains indices in the conditional_stack_.
  util_intops::StrongVector<LinearExpression2Index, int> conditional_relations_;

  // Store for each variable x, the variables y that appears alongside it in
  // lit => x + y <= ub. Note that conditional_var_lookup_ is updated on
  // dive/backtrack.
  util_intops::StrongVector<
      IntegerVariable,
      std::vector<std::pair<IntegerVariable, LinearExpression2Index>>>
      conditional_var_lookup_;

  // Temp data for CollectPrecedences.
  std::vector<IntegerVariable> var_with_positive_degree_;
  util_intops::StrongVector<IntegerVariable, int> var_to_degree_;
  util_intops::StrongVector<IntegerVariable, int> var_to_last_index_;
  std::vector<PrecedenceData> tmp_precedences_;
};

// A relation of the form enforcement => expr \in [lhs, rhs].
// Note that the [lhs, rhs] interval should always be within [min_activity,
// max_activity] where the activity is the value of expr.
struct Relation {
  Literal enforcement;
  LinearExpression2 expr;
  IntegerValue lhs;
  IntegerValue rhs;

  bool operator==(const Relation& other) const {
    return enforcement == other.enforcement && expr == other.expr &&
           lhs == other.lhs && rhs == other.rhs;
  }

  template <typename Sink>
  friend void AbslStringify(Sink& sink, const Relation& relation) {
    absl::Format(&sink, "%s => %v in [%v, %v]",
                 relation.enforcement.DebugString(), relation.expr,
                 relation.lhs, relation.rhs);
  }
};

class ReifiedLinear2Bounds;

// A repository of all the enforced linear constraints of size 1 or 2.
//
// TODO(user): This is not always needed, find a way to clean this once we
// don't need it.
class BinaryRelationRepository {
 public:
  explicit BinaryRelationRepository(Model* model)
      : reified_linear2_bounds_(model->GetOrCreate<ReifiedLinear2Bounds>()),
        shared_stats_(model->GetOrCreate<SharedStatistics>()) {}
  ~BinaryRelationRepository();

  int size() const { return relations_.size(); }

  // The returned relation is guaranteed to only have positive variables.
  const Relation& relation(int index) const { return relations_[index]; }

  // Returns the indices of the relations that are enforced by the given
  // literal.
  absl::Span<const int> IndicesOfRelationsEnforcedBy(LiteralIndex lit) const {
    if (lit >= lit_to_relations_.size()) return {};
    return lit_to_relations_[lit];
  }

  // Adds a conditional relation lit => expr \in [lhs, rhs] (one of the coeffs
  // can be zero).
  void Add(Literal lit, LinearExpression2 expr, IntegerValue lhs,
           IntegerValue rhs);

  // Adds a partial conditional relation between two variables, with unspecified
  // coefficients and bounds.
  void AddPartialRelation(Literal lit, IntegerVariable a, IntegerVariable b);

  // Builds the literal to relations mapping. This should be called once all the
  // relations have been added.
  void Build();

 private:
  ReifiedLinear2Bounds* reified_linear2_bounds_;
  SharedStatistics* shared_stats_;
  bool is_built_ = false;
  int num_enforced_relations_ = 0;
  int num_encoded_equivalences_ = 0;
  std::vector<Relation> relations_;
  CompactVectorVector<LiteralIndex, int> lit_to_relations_;
};

// Class that keeps the best upper bound for a*x + b*y by using all the linear3
// relations of the form a*x + b*y + c*z <= d.
class Linear2BoundsFromLinear3 {
 public:
  explicit Linear2BoundsFromLinear3(Model* model);
  ~Linear2BoundsFromLinear3();

  // If the given upper bound evaluate better than the current one we have, this
  // will replace it and returns true, otherwise it returns false.
  bool AddAffineUpperBound(LinearExpression2Index lin2_index,
                           IntegerValue lin_expr_gcd,
                           AffineExpression affine_ub);

  bool AddAffineUpperBound(LinearExpression2 expr, AffineExpression affine_ub) {
    expr.SimpleCanonicalization();
    if (expr.coeffs[0] == 0 || expr.coeffs[1] == 0) return false;
    const IntegerValue gcd = expr.DivideByGcd();
    return AddAffineUpperBound(lin2_indices_->AddOrGet(expr), gcd, affine_ub);
  }

  // Most users should just use Linear2Bounds::UpperBound() instead.
  //
  // Returns the upper bound only if there is some relations coming from a
  // linear3. Otherwise always returns kMaxIntegerValue.
  // `expr` must be canonicalized and gcd-reduced.
  IntegerValue GetUpperBoundFromLinear3(
      LinearExpression2Index lin2_index) const;

  // Most users should use Linear2Bounds::AddReasonForUpperBoundLowerThan()
  // instead.
  //
  // Adds the reason for GetUpperBoundFromLinear3() to be <= ub.
  // `expr` must be canonicalized and gcd-reduced.
  void AddReasonForUpperBoundLowerThan(
      LinearExpression2Index lin2_index, IntegerValue ub,
      std::vector<Literal>* literal_reason,
      std::vector<IntegerLiteral>* integer_reason) const;

 private:
  IntegerTrail* integer_trail_;
  Trail* trail_;
  Linear2Watcher* linear2_watcher_;
  GenericLiteralWatcher* watcher_;
  SharedStatistics* shared_stats_;
  RootLevelLinear2Bounds* root_level_bounds_;
  Linear2Indices* lin2_indices_;

  int64_t num_affine_updates_ = 0;

  // This stores linear2 <= AffineExpression / divisor.
  //
  // Note(user): This is a "cheap way" to not have to deal with backtracking, If
  // we have many possible AffineExpression that bounds a LinearExpression2, we
  // keep the best one during "search dive" but on backtrack we might have a
  // sub-optimal relation.
  util_intops::StrongVector<LinearExpression2Index,
                            std::pair<AffineExpression, IntegerValue>>
      best_affine_ub_;
};

// TODO(user): Merge with BinaryRelationRepository. Note that this one provides
// different indexing though, so it could be kept separate.
class ReifiedLinear2Bounds {
 public:
  explicit ReifiedLinear2Bounds(Model* model);
  ~ReifiedLinear2Bounds();

  // Register the fact that l <=> ( expr <= ub ).
  // `expr` must already be canonicalized and gcd-reduced.
  // These are considered equivalence relation.
  void AddBoundEncodingIfNonTrivial(Literal l, LinearExpression2 expr,
                                    IntegerValue ub);

  // Add a linear3 of the form vars[i]*coeffs[i] = activity that is not
  // enforced and valid at level zero.
  void AddLinear3(absl::Span<const IntegerVariable> vars,
                  absl::Span<const int64_t> coeffs, int64_t activity);

  // Returns ReifiedBoundType if we don't have a literal <=> ( expr <= ub ), or
  // returns that literal if we have one. `expr` must be canonicalized and
  // gcd-reduced.
  enum class ReifiedBoundType {
    kNoLiteralStored,
    kAlwaysTrue,
    kAlwaysFalse,
  };
  std::variant<ReifiedBoundType, Literal, IntegerLiteral> GetEncodedBound(
      LinearExpression2 expr, IntegerValue ub);

  std::pair<AffineExpression, IntegerValue> GetLinear3Bound(
      LinearExpression2Index lin2_index) const;

 private:
  RootLevelLinear2Bounds* best_root_level_bounds_;
  Linear2Indices* lin2_indices_;
  SharedStatistics* shared_stats_;

  // This stores divisor * linear2 = AffineExpression similarly to
  // Linear2BoundsFromLinear3. The difference here is that we only store linear3
  // that are equality, but irrespective of whether it constraint any linear2 at
  // the current level. If there is more than one expression for a given
  // linear2, we will keep the one with the smallest divisor.
  util_intops::StrongVector<LinearExpression2Index,
                            std::pair<AffineExpression, IntegerValue>>
      linear3_bounds_;

  // This stores relations l <=> (linear2 <= rhs).
  absl::flat_hash_map<std::pair<LinearExpression2Index, IntegerValue>, Literal>
      relation_to_lit_;

  // This is used to detect relations that become fixed at level zero and
  // "upgrade" them to non-enforced relations. Because we only do that when
  // we fix variable, a linear scan shouldn't be too bad and is relatively
  // compact memory wise.
  absl::flat_hash_set<BooleanVariable> variable_appearing_in_reified_relations_;
  std::vector<std::tuple<Literal, LinearExpression2Index, IntegerValue>>
      all_reified_relations_;

  int64_t num_linear3_relations_ = 0;
  int64_t num_relations_fixed_at_root_level_ = 0;
};

// Simple wrapper around the different repositories for bounds of linear2.
// This should provide the best bounds.
class Linear2Bounds : public LazyReasonInterface {
 public:
  explicit Linear2Bounds(Model* model)
      : integer_trail_(model->GetOrCreate<IntegerTrail>()),
        root_level_bounds_(model->GetOrCreate<RootLevelLinear2Bounds>()),
        enforced_bounds_(model->GetOrCreate<EnforcedLinear2Bounds>()),
        linear3_bounds_(model->GetOrCreate<Linear2BoundsFromLinear3>()),
        lin2_indices_(model->GetOrCreate<Linear2Indices>()),
        reified_lin2_bounds_(model->GetOrCreate<ReifiedLinear2Bounds>()),
        trail_(model->GetOrCreate<Trail>()),
        shared_stats_(model->GetOrCreate<SharedStatistics>()) {}

  ~Linear2Bounds();

  // Returns the best known upper-bound of the given LinearExpression2 at the
  // current decision level. If its explanation is needed, it can be queried
  // with the second function.
  IntegerValue UpperBound(LinearExpression2 expr) const;
  IntegerValue UpperBound(LinearExpression2Index lin2_index) const;

  void AddReasonForUpperBoundLowerThan(
      LinearExpression2 expr, IntegerValue ub,
      std::vector<Literal>* literal_reason,
      std::vector<IntegerLiteral>* integer_reason) const;

  RelationStatus GetStatus(LinearExpression2 expr, IntegerValue lb,
                           IntegerValue ub) const;

  // Like UpperBound() but do not consider the bounds coming from
  // the individual variable bounds. This is faster.
  IntegerValue NonTrivialUpperBound(LinearExpression2Index lin2_index) const;

  // Given the new linear2 bounds and its reason, inspect our various repository
  // to find the strongest way to push this new upper bound.
  bool EnqueueLowerOrEqual(LinearExpression2 expr, IntegerValue ub,
                           absl::Span<const Literal> literal_reason,
                           absl::Span<const IntegerLiteral> integer_reason);

  // For LazyReasonInterface.
  std::string LazyReasonName() const final { return "Linear2Bounds"; }
  void Explain(int id, IntegerLiteral to_explain, IntegerReason* reason) final;

 private:
  IntegerTrail* integer_trail_;
  RootLevelLinear2Bounds* root_level_bounds_;
  EnforcedLinear2Bounds* enforced_bounds_;
  Linear2BoundsFromLinear3* linear3_bounds_;
  Linear2Indices* lin2_indices_;
  ReifiedLinear2Bounds* reified_lin2_bounds_;
  Trail* trail_;
  SharedStatistics* shared_stats_;

  // This is used for the lazy-reason implemented in Explain().
  util_intops::StrongVector<ReasonIndex, LinearExpression2> saved_reasons_;

  int64_t enqueue_trivial_ = 0;
  int64_t enqueue_degenerate_ = 0;
  int64_t enqueue_true_at_root_level_ = 0;
  int64_t enqueue_conflict_false_at_root_level_ = 0;
  int64_t enqueue_individual_var_bounds_ = 0;
  int64_t enqueue_literal_encoding_ = 0;
  int64_t enqueue_integer_linear3_encoding_ = 0;
};

// Assuming level-zero bounds + any (var >= value) in the input map,
// fills "output" with a "propagated" set of bounds assuming lit is true (by
// using the relations enforced by lit, as well as the non-enforced ones).
// Note that we will only fill bounds > level-zero ones in output.
//
// Returns false if the new bounds are infeasible at level zero.
//
// Important: by default this does not call output->clear() so we can take
// the max with already inferred bounds.
bool PropagateLocalBounds(
    const IntegerTrail& integer_trail,
    const RootLevelLinear2Bounds& root_level_bounds,
    const BinaryRelationRepository& repository,
    const ImpliedBounds& implied_bounds, Literal lit,
    const absl::flat_hash_map<IntegerVariable, IntegerValue>& input,
    absl::flat_hash_map<IntegerVariable, IntegerValue>* output);

// Detects if at least one of a subset of linear of size 2 or 1, touching the
// same variable, must be true. When this is the case we add a new propagator to
// propagate that fact.
//
// TODO(user): Shall we do that on the main thread before the workers are
// spawned? note that the probing version need the model to be loaded though.
class GreaterThanAtLeastOneOfDetector {
 public:
  explicit GreaterThanAtLeastOneOfDetector(Model* model)
      : repository_(*model->GetOrCreate<BinaryRelationRepository>()),
        implied_bounds_(*model->GetOrCreate<ImpliedBounds>()),
        integer_trail_(*model->GetOrCreate<IntegerTrail>()),
        shared_stats_(model->GetOrCreate<SharedStatistics>()) {}

  ~GreaterThanAtLeastOneOfDetector();

  // Advanced usage. To be called once all the constraints have been added to
  // the model. This will detect GreaterThanAtLeastOneOfConstraint().
  // Returns the number of added constraint.
  //
  // TODO(user): This can be quite slow, add some kind of deterministic limit
  // so that we can use it all the time.
  int AddGreaterThanAtLeastOneOfConstraints(Model* model,
                                            bool auto_detect_clauses = false);

 private:
  struct VariableConditionalAffineBound {
    Literal enforcement_literal;
    IntegerVariable var;
    AffineExpression bound;
  };

  void AddRelationIfNonTrivial(
      const Relation& r,
      std::vector<VariableConditionalAffineBound>* clause_bounds) const;

  // Given an existing clause, sees if it can be used to add "greater than at
  // least one of" type of constraints. Returns the number of such constraint
  // added.
  int AddGreaterThanAtLeastOneOfConstraintsFromClause(
      absl::Span<const Literal> clause, Model* model,
      const CompactVectorVector<LiteralIndex, IntegerLiteral>&
          implied_bounds_by_literal);

  // Another approach for AddGreaterThanAtLeastOneOfConstraints(), this one
  // might be a bit slow as it relies on the propagation engine to detect
  // clauses between incoming arcs presence literals.
  // Returns the number of added constraints.
  int AddGreaterThanAtLeastOneOfConstraintsWithClauseAutoDetection(
      Model* model);

  // Once we identified a clause and relevant indices, this build the
  // constraint. Returns true if we actually add it.
  bool AddRelationFromBounds(
      IntegerVariable var, absl::Span<const Literal> clause,
      absl::Span<const VariableConditionalAffineBound> bounds, Model* model);

  BinaryRelationRepository& repository_;
  const ImpliedBounds& implied_bounds_;
  IntegerTrail& integer_trail_;
  SharedStatistics* shared_stats_;
  int64_t num_detected_constraints_ = 0;
  int64_t total_num_expressions_ = 0;
  int64_t maximum_num_expressions_ = 0;
};

// This can be in a hot-loop, so we want to inline it if possible.
inline IntegerValue Linear2Bounds::NonTrivialUpperBound(
    LinearExpression2Index lin2_index) const {
  CHECK_NE(lin2_index, kNoLinearExpression2Index);
  IntegerValue ub = kMaxIntegerValue;
  ub = std::min(ub, root_level_bounds_->GetUpperBoundNoTrail(lin2_index));
  ub = std::min(ub, enforced_bounds_->GetUpperBoundFromEnforced(lin2_index));
  ub = std::min(ub, linear3_bounds_->GetUpperBoundFromLinear3(lin2_index));
  return ub;
}
inline LinearExpression2Index Linear2Indices::GetIndex(
    LinearExpression2 expr) const {
  if (expr.coeffs[0] == 0 || expr.coeffs[1] == 0) {
    return kNoLinearExpression2Index;
  }
  DCHECK(expr.IsCanonicalized());
  DCHECK_EQ(expr.DivideByGcd(), 1);
  const bool negated = expr.NegateForCanonicalization();
  auto it = expr_to_index_.find(expr);
  if (it == expr_to_index_.end()) return kNoLinearExpression2Index;

  const LinearExpression2Index positive_index(2 * it->second);
  if (negated) {
    return NegationOf(positive_index);
  } else {
    return positive_index;
  }
}

inline LinearExpression2 Linear2Indices::GetExpression(
    LinearExpression2Index index) const {
  DCHECK_NE(index, kNoLinearExpression2Index);
  const int lookup_index = index.value() / 2;
  DCHECK_LT(lookup_index, exprs_.size());
  if (Linear2IsPositive(index)) {
    return exprs_[lookup_index];
  } else {
    LinearExpression2 result = exprs_[lookup_index];
    result.Negate();
    return result;
  }
}

inline absl::Span<const LinearExpression2Index>
Linear2Indices::GetAllLinear2ContainingVariable(IntegerVariable var) const {
  const IntegerVariable positive_var = PositiveVariable(var);
  auto it = var_to_bounds_.find(positive_var);
  if (it == var_to_bounds_.end()) return {};
  return it->second;
}

inline absl::Span<const LinearExpression2Index>
Linear2Indices::GetAllLinear2ContainingVariables(IntegerVariable var1,
                                                 IntegerVariable var2) const {
  IntegerVariable positive_var1 = PositiveVariable(var1);
  IntegerVariable positive_var2 = PositiveVariable(var2);
  if (positive_var1 > positive_var2) {
    std::swap(positive_var1, positive_var2);
  }
  auto it = var_pair_to_bounds_.find({positive_var1, positive_var2});
  if (it == var_pair_to_bounds_.end()) return {};
  return it->second;
}

}  // namespace sat
}  // namespace operations_research

#endif  // OR_TOOLS_SAT_PRECEDENCES_H_
