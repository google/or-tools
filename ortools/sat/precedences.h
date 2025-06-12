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
#include <deque>
#include <functional>
#include <tuple>
#include <utility>
#include <vector>

#include "absl/container/btree_set.h"
#include "absl/container/flat_hash_map.h"
#include "absl/container/flat_hash_set.h"
#include "absl/container/inlined_vector.h"
#include "absl/log/check.h"
#include "absl/strings/str_format.h"
#include "absl/types/span.h"
#include "ortools/base/strong_vector.h"
#include "ortools/graph/graph.h"
#include "ortools/sat/integer.h"
#include "ortools/sat/integer_base.h"
#include "ortools/sat/model.h"
#include "ortools/sat/sat_base.h"
#include "ortools/sat/sat_solver.h"
#include "ortools/sat/synchronization.h"
#include "ortools/sat/util.h"
#include "ortools/util/bitset.h"
#include "ortools/util/rev.h"
#include "ortools/util/strong_integers.h"

namespace operations_research {
namespace sat {

// This holds all the relation lhs <= linear2 <= rhs that are true at level
// zero. It is the source of truth across all the solver for such bounds.
class RootLevelLinear2Bounds {
 public:
  explicit RootLevelLinear2Bounds(Model* model)
      : integer_trail_(model->GetOrCreate<IntegerTrail>()),
        shared_stats_(model->GetOrCreate<SharedStatistics>()) {}

  ~RootLevelLinear2Bounds();

  // Add a relation lb <= expr <= ub. If expr is not a proper linear2 expression
  // (e.g. 0*x + y, y + y, y - y) it will be ignored.
  // Returns a pair saying whether the lower/upper bounds for this expr became
  // more restricted than what was currently stored.
  std::pair<bool, bool> Add(LinearExpression2 expr, IntegerValue lb,
                            IntegerValue ub);

  // Same as above, but only update the upper bound.
  bool AddUpperBound(LinearExpression2 expr, IntegerValue ub) {
    return Add(expr, kMinIntegerValue, ub).second;
  }

  IntegerValue LevelZeroUpperBound(LinearExpression2 expr) const;

  int64_t num_bounds() const { return root_level_relations_.num_bounds(); }

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
  absl::Span<const IntegerVariable> GetVariablesInSimpleRelation(
      IntegerVariable var) const {
    if (var >= coeff_one_var_lookup_.size()) return {};
    return coeff_one_var_lookup_[var];
  }

  RelationStatus GetLevelZeroStatus(LinearExpression2 expr, IntegerValue lb,
                                    IntegerValue ub) const;

  // Low-level function that returns the zero-level upper bound if it is
  // non-trivial. Otherwise returns kMaxIntegerValue. This is a different
  // behavior from LevelZeroUpperBound() that would return the implied
  // zero-level bound from the trail for trivial ones. `expr` must be
  // canonicalized and gcd-reduced.
  IntegerValue GetUpperBoundNoTrail(LinearExpression2 expr) const;

 private:
  IntegerTrail* integer_trail_;
  SharedStatistics* shared_stats_;

  // Lookup table to find all the LinearExpression2 with a given variable and
  // having both coefficient 1.
  util_intops::StrongVector<IntegerVariable, std::vector<IntegerVariable>>
      coeff_one_var_lookup_;

  // TODO(user): use data structures that consume less memory. A single
  // std::vector<LinearExpression2> and hash maps having the index as value
  // could be enough.
  absl::flat_hash_map<
      IntegerVariable,
      absl::InlinedVector<
          std::tuple<LinearExpression2, IntegerValue, IntegerValue>, 2>>
      var_to_bounds_;
  // Map to implement GetAllBoundsContainingVariables().
  absl::flat_hash_map<
      std::pair<IntegerVariable, IntegerVariable>,
      absl::InlinedVector<
          std::tuple<LinearExpression2, IntegerValue, IntegerValue>, 1>>
      var_pair_to_bounds_;
  // Data structure to quickly update var_to_bounds_. Return the index where
  // this linear expression appear in the vector for the first and second
  // variable.
  absl::flat_hash_map<LinearExpression2, std::pair<int, int>>
      var_to_bounds_vector_index_;
  absl::flat_hash_map<LinearExpression2, int> var_pair_to_bounds_vector_index_;

  // TODO(user): Also push them to a global shared repository after
  // remapping IntegerVariable to proto indices.
  BestBinaryRelationBounds root_level_relations_;
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
// TODO(user): Support conditional relation.
// TODO(user): Support non-DAG like graph.
// TODO(user): Support variable offset that can be updated as search progress.
class TransitivePrecedencesEvaluator {
 public:
  explicit TransitivePrecedencesEvaluator(Model* model)
      : integer_trail_(model->GetOrCreate<IntegerTrail>()),
        shared_stats_(model->GetOrCreate<SharedStatistics>()),
        root_level_bounds_(model->GetOrCreate<RootLevelLinear2Bounds>()) {}

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
  // Warning: If there are too many, this will NOT contain all relations.
  //
  // Returns kMaxIntegerValue if there are none, otherwise return an upper bound
  // such that expr <= ub.
  //
  // TODO(user): Be more dynamic as we start to add relations during search.
  void Build();

 private:
  IntegerTrail* integer_trail_;
  SharedStatistics* shared_stats_;
  RootLevelLinear2Bounds* root_level_bounds_;

  util::StaticGraph<> graph_;
  std::vector<IntegerValue> arc_offsets_;

  bool is_built_ = false;
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
        root_level_bounds_(model->GetOrCreate<RootLevelLinear2Bounds>()),
        shared_stats_(model->GetOrCreate<SharedStatistics>()) {
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
                               LinearExpression2 expr, IntegerValue rhs);

  // Called each time we change decision level.
  void SetLevel(int level) final;

  // Returns a set of precedences (var, index) such that we have a relation
  // of the form var[index] <= var + offset.
  //
  // All entries for the same variable will be contiguous and sorted by index.
  // We only list variable with at least two entries. The offset can be
  // retrieved via Linear2Bounds::UpperBound(Difference(vars[index]), var)).
  //
  // This method currently ignores all linear2 expressions with any coefficient
  // different from 1.
  //
  // TODO(user): Ideally this should be moved to a new class and maybe augmented
  // with other kind of precedences.
  struct PrecedenceData {
    IntegerVariable var;
    int index;
  };
  void CollectPrecedences(absl::Span<const IntegerVariable> vars,
                          std::vector<PrecedenceData>* output);

  // Low-level function that returns the upper bound if there is some enforced
  // relations only. Otherwise always returns kMaxIntegerValue.
  // `expr` must be canonicalized and gcd-reduced.
  IntegerValue GetUpperBoundFromEnforced(LinearExpression2 expr) const;

  void AddReasonForUpperBoundLowerThan(
      LinearExpression2 expr, IntegerValue ub,
      std::vector<Literal>* literal_reason,
      std::vector<IntegerLiteral>* integer_reason) const;

  // Note: might contain duplicate expressions.
  std::vector<LinearExpression2> GetAllExpressionsWithConditionalBounds() const;

 private:
  void CreateLevelEntryIfNeeded();

  const SatParameters& params_;
  Trail* trail_;
  IntegerTrail* integer_trail_;
  RootLevelLinear2Bounds* root_level_bounds_;
  SharedStatistics* shared_stats_;

  int64_t num_conditional_relation_updates_ = 0;

  // Conditional stack for push/pop of conditional relations.
  //
  // TODO(user): this kind of reversible hash_map is already implemented in
  // other part of the code. Consolidate.
  struct ConditionalEntry {
    ConditionalEntry(int p, IntegerValue r, LinearExpression2 k,
                     absl::Span<const Literal> e)
        : prev_entry(p), rhs(r), key(k), enforcements(e.begin(), e.end()) {}

    int prev_entry;
    IntegerValue rhs;
    LinearExpression2 key;
    absl::InlinedVector<Literal, 4> enforcements;
  };
  std::vector<ConditionalEntry> conditional_stack_;
  std::vector<std::pair<int, int>> level_to_stack_size_;

  // This is always stored in the form (expr <= rhs).
  // The conditional relations contains indices in the conditional_stack_.
  absl::flat_hash_map<LinearExpression2, int> conditional_relations_;

  // Store for each variable x, the variables y that appears alongside it in
  // lit => x + y <= ub. Note that conditional_var_lookup_ is updated on
  // dive/backtrack.
  util_intops::StrongVector<IntegerVariable, std::vector<IntegerVariable>>
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

// A repository of all the enforced linear constraints of size 1 or 2.
//
// TODO(user): This is not always needed, find a way to clean this once we
// don't need it.
class BinaryRelationRepository {
 public:
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
      const RootLevelLinear2Bounds& root_level_bounds, Literal lit,
      const absl::flat_hash_map<IntegerVariable, IntegerValue>& input,
      absl::flat_hash_map<IntegerVariable, IntegerValue>* output) const;

 private:
  bool is_built_ = false;
  int num_enforced_relations_ = 0;
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
  //
  // Note that we never store trivial upper bound (using the current variable
  // domain).
  bool AddAffineUpperBound(LinearExpression2 expr, AffineExpression affine_ub);

  // Returns the best known upper-bound of the given LinearExpression2 at the
  // current decision level. If its explanation is needed, it can be queried
  // with the second function.
  //
  // NOTE: most users will want to call Linear2Bounds::UpperBound() instead.
  IntegerValue UpperBound(LinearExpression2 expr) const;
  void AddReasonForUpperBoundLowerThan(
      LinearExpression2 expr, IntegerValue ub,
      std::vector<Literal>* literal_reason,
      std::vector<IntegerLiteral>* integer_reason) const;

  // Warning, the order will not be deterministic.
  std::vector<LinearExpression2> GetAllExpressionsWithAffineBounds() const;

  int NumExpressionsWithAffineBounds() const { return best_affine_ub_.size(); }

  void WatchAllLinearExpressions2(int id) { propagator_ids_.insert(id); }

  // Low-level function that returns the upper bound only if there is some
  // relations coming from a linear3. Otherwise always returns kMaxIntegerValue.
  // `expr` must be canonicalized and gcd-reduced.
  IntegerValue GetUpperBoundFromLinear3(LinearExpression2 expr) const;

 private:
  void NotifyWatchingPropagators() const;

  IntegerTrail* integer_trail_;
  Trail* trail_;
  GenericLiteralWatcher* watcher_;
  SharedStatistics* shared_stats_;
  RootLevelLinear2Bounds* best_root_level_bounds_;

  int64_t num_affine_updates_ = 0;

  // This stores linear2 <= AffineExpression / divisor.
  //
  // Note(user): This is a "cheap way" to not have to deal with backtracking, If
  // we have many possible AffineExpression that bounds a LinearExpression2, we
  // keep the best one during "search dive" but on backtrack we might have a
  // sub-optimal relation.
  absl::flat_hash_map<LinearExpression2,
                      std::pair<AffineExpression, IntegerValue>>
      best_affine_ub_;

  absl::btree_set<int> propagator_ids_;
};

// TODO(user): Merge with BinaryRelationRepository. Note that this one provides
// different indexing though, so it could be kept separate.
// TODO(user): Use LinearExpression2 instead of pairs of AffineExpression for
// consistency with other classes.
class ReifiedLinear2Bounds {
 public:
  explicit ReifiedLinear2Bounds(Model* model);

  // Return the status of a <= b;
  RelationStatus GetLevelZeroPrecedenceStatus(AffineExpression a,
                                              AffineExpression b) const;

  // Register the fact that l <=> ( a <= b ).
  // These are considered equivalence relation.
  void AddReifiedPrecedenceIfNonTrivial(Literal l, AffineExpression a,
                                        AffineExpression b);

  // Returns kNoLiteralIndex if we don't have a literal <=> ( a <= b ), or
  // returns that literal if we have one. Note that we will return the
  // true/false literal if the status is known at level zero.
  LiteralIndex GetReifiedPrecedence(AffineExpression a, AffineExpression b);

 private:
  IntegerEncoder* integer_encoder_;
  RootLevelLinear2Bounds* best_root_level_bounds_;

  // This stores relations l <=> (linear2 <= rhs).
  absl::flat_hash_map<std::pair<LinearExpression2, IntegerValue>, Literal>
      relation_to_lit_;

  // This is used to detect relations that become fixed at level zero and
  // "upgrade" them to non-enforced relations. Because we only do that when
  // we fix variable, a linear scan shouldn't be too bad and is relatively
  // compact memory wise.
  absl::flat_hash_set<BooleanVariable> variable_appearing_in_reified_relations_;
  std::vector<std::tuple<Literal, LinearExpression2, IntegerValue>>
      all_reified_relations_;
};

// Simple wrapper around the different repositories for bounds of linear2.
// This should provide the best bounds.
class Linear2Bounds {
 public:
  explicit Linear2Bounds(Model* model)
      : root_level_bounds_(model->GetOrCreate<RootLevelLinear2Bounds>()),
        integer_trail_(model->GetOrCreate<IntegerTrail>()),
        enforced_bounds_(model->GetOrCreate<EnforcedLinear2Bounds>()),
        linear3_bounds_(model->GetOrCreate<Linear2BoundsFromLinear3>()) {}

  // Returns the best known upper-bound of the given LinearExpression2 at the
  // current decision level. If its explanation is needed, it can be queried
  // with the second function.
  IntegerValue UpperBound(LinearExpression2 expr) const;
  void AddReasonForUpperBoundLowerThan(
      LinearExpression2 expr, IntegerValue ub,
      std::vector<Literal>* literal_reason,
      std::vector<IntegerLiteral>* integer_reason) const;

  // Like UpperBound(), but optimized for the case of gcd == 1 and when we
  // don't want the trivial bounds.
  IntegerValue NonTrivialUpperBoundForGcd1(LinearExpression2 expr) const;

  std::vector<LinearExpression2>
  GetAllExpressionsWithPotentialNonTrivialBounds() const;

 private:
  RootLevelLinear2Bounds* root_level_bounds_;
  IntegerTrail* integer_trail_;
  EnforcedLinear2Bounds* enforced_bounds_;
  Linear2BoundsFromLinear3* linear3_bounds_;
};

// Detects if at least one of a subset of linear of size 2 or 1, touching the
// same variable, must be true. When this is the case we add a new propagator to
// propagate that fact.
//
// TODO(user): Shall we do that on the main thread before the workers are
// spawned? note that the probing version need the model to be loaded though.
class GreaterThanAtLeastOneOfDetector {
 public:
  explicit GreaterThanAtLeastOneOfDetector(Model* model)
      : repository_(*model->GetOrCreate<BinaryRelationRepository>()) {}

  // Advanced usage. To be called once all the constraints have been added to
  // the model. This will detect GreaterThanAtLeastOneOfConstraint().
  // Returns the number of added constraint.
  //
  // TODO(user): This can be quite slow, add some kind of deterministic limit
  // so that we can use it all the time.
  int AddGreaterThanAtLeastOneOfConstraints(Model* model,
                                            bool auto_detect_clauses = false);

 private:
  // Given an existing clause, sees if it can be used to add "greater than at
  // least one of" type of constraints. Returns the number of such constraint
  // added.
  int AddGreaterThanAtLeastOneOfConstraintsFromClause(
      absl::Span<const Literal> clause, Model* model);

  // Another approach for AddGreaterThanAtLeastOneOfConstraints(), this one
  // might be a bit slow as it relies on the propagation engine to detect
  // clauses between incoming arcs presence literals.
  // Returns the number of added constraints.
  int AddGreaterThanAtLeastOneOfConstraintsWithClauseAutoDetection(
      Model* model);

  // Once we identified a clause and relevant indices, this build the
  // constraint. Returns true if we actually add it.
  bool AddRelationFromIndices(IntegerVariable var,
                              absl::Span<const Literal> clause,
                              absl::Span<const int> indices, Model* model);

  BinaryRelationRepository& repository_;
};

// =============================================================================
// Old precedences propagator.
//
// This is superseded by the new LinearPropagator and should only be used if the
// option 'new_linear_propagation' is false. We still keep it around to
// benchmark and test the new code vs this one.
// =============================================================================

// This class implement a propagator on simple inequalities between integer
// variables of the form (i1 + offset <= i2). The offset can be constant or
// given by the value of a third integer variable. Offsets can also be negative.
//
// The algorithm works by mapping the problem onto a graph where the edges carry
// the offset and the nodes correspond to one of the two bounds of an integer
// variable (lower_bound or -upper_bound). It then find the fixed point using an
// incremental variant of the Bellman-Ford(-Tarjan) algorithm.
//
// This is also known as an "integer difference logic theory" in the SMT world.
// Another word is "separation logic".
//
// TODO(user): We could easily generalize the code to support any relation of
// the form a*X + b*Y + c*Z >= rhs (or <=). Do that since this class should be
// a lot faster at propagating small linear inequality than the generic
// propagator and the overhead of supporting coefficient should not be too bad.
class PrecedencesPropagator : public SatPropagator, PropagatorInterface {
 public:
  explicit PrecedencesPropagator(Model* model)
      : SatPropagator("PrecedencesPropagator"),
        relations_(model->GetOrCreate<EnforcedLinear2Bounds>()),
        trail_(model->GetOrCreate<Trail>()),
        integer_trail_(model->GetOrCreate<IntegerTrail>()),
        shared_stats_(model->Mutable<SharedStatistics>()),
        watcher_(model->GetOrCreate<GenericLiteralWatcher>()),
        watcher_id_(watcher_->Register(this)) {
    model->GetOrCreate<SatSolver>()->AddPropagator(this);
    integer_trail_->RegisterWatcher(&modified_vars_);
    watcher_->SetPropagatorPriority(watcher_id_, 0);
  }

  // This type is neither copyable nor movable.
  PrecedencesPropagator(const PrecedencesPropagator&) = delete;
  PrecedencesPropagator& operator=(const PrecedencesPropagator&) = delete;
  ~PrecedencesPropagator() override;

  bool Propagate() final;
  bool Propagate(Trail* trail) final;
  void Untrail(const Trail& trail, int trail_index) final;

  // Propagates all the outgoing arcs of the given variable (and only those). It
  // is more efficient to do all these propagation in one go by calling
  // Propagate(), but for scheduling problem, we wants to propagate right away
  // the end of an interval when its start moved.
  bool PropagateOutgoingArcs(IntegerVariable var);

  // Add a precedence relation (i1 + offset <= i2) between integer variables.
  //
  // Important: The optionality of the variable should be marked BEFORE this
  // is called.
  void AddPrecedence(IntegerVariable i1, IntegerVariable i2);
  void AddPrecedenceWithOffset(IntegerVariable i1, IntegerVariable i2,
                               IntegerValue offset);
  void AddPrecedenceWithVariableOffset(IntegerVariable i1, IntegerVariable i2,
                                       IntegerVariable offset_var);

  // Same as above, but the relation is only true when the given literal is.
  void AddConditionalPrecedence(IntegerVariable i1, IntegerVariable i2,
                                Literal l);
  void AddConditionalPrecedenceWithOffset(IntegerVariable i1,
                                          IntegerVariable i2,
                                          IntegerValue offset, Literal l);

  // Generic function that cover all of the above case and more.
  void AddPrecedenceWithAllOptions(IntegerVariable i1, IntegerVariable i2,
                                   IntegerValue offset,
                                   IntegerVariable offset_var,
                                   absl::Span<const Literal> presence_literals);

  // This version check current precedence. It is however "slow".
  bool AddPrecedenceWithOffsetIfNew(IntegerVariable i1, IntegerVariable i2,
                                    IntegerValue offset);

 private:
  DEFINE_STRONG_INDEX_TYPE(ArcIndex);
  DEFINE_STRONG_INDEX_TYPE(OptionalArcIndex);

  // Information about an individual arc.
  struct ArcInfo {
    IntegerVariable tail_var;
    IntegerVariable head_var;

    IntegerValue offset;
    IntegerVariable offset_var;  // kNoIntegerVariable if none.

    // This arc is "present" iff all these literals are true.
    absl::InlinedVector<Literal, 6> presence_literals;

    // Used temporarily by our implementation of the Bellman-Ford algorithm. It
    // should be false at the beginning of BellmanFordTarjan().
    mutable bool is_marked;
  };

  // Internal functions to add new precedence relations.
  //
  // Note that internally, we only propagate lower bounds, so each time we add
  // an arc, we actually create two of them: one on the given variables, and one
  // on their negation.
  void AdjustSizeFor(IntegerVariable i);
  void AddArc(IntegerVariable tail, IntegerVariable head, IntegerValue offset,
              IntegerVariable offset_var,
              absl::Span<const Literal> presence_literals);

  // Enqueue a new lower bound for the variable arc.head_lb that was deduced
  // from the current value of arc.tail_lb and the offset of this arc.
  bool EnqueueAndCheck(const ArcInfo& arc, IntegerValue new_head_lb,
                       Trail* trail);
  IntegerValue ArcOffset(const ArcInfo& arc) const;

  // Inspect all the optional arcs that needs inspection (to stay sparse) and
  // check if their presence literal can be propagated to false.
  void PropagateOptionalArcs(Trail* trail);

  // The core algorithm implementation is split in these functions. One must
  // first call InitializeBFQueueWithModifiedNodes() that will push all the
  // IntegerVariable whose lower bound has been modified since the last call.
  // Then, BellmanFordTarjan() will take care of all the propagation and returns
  // false in case of conflict. Internally, it uses DisassembleSubtree() which
  // is the Tarjan variant to detect a possible positive cycle. Before exiting,
  // it will call CleanUpMarkedArcsAndParents().
  //
  // The Tarjan version of the Bellam-Ford algorithm is really nice in our
  // context because it was really easy to make it incremental. Moreover, it
  // supports batch increment!
  //
  // This implementation is kind of unique because of our context and the fact
  // that it is incremental, but a good reference is "Negative-cycle detection
  // algorithms", Boris V. Cherkassky, Andrew V. Goldberg, 1996,
  // http://people.cs.nctu.edu.tw/~tjshen/doc/ne.pdf
  void InitializeBFQueueWithModifiedNodes();
  bool BellmanFordTarjan(Trail* trail);
  bool DisassembleSubtree(int source, int target,
                          std::vector<bool>* can_be_skipped);
  void AnalyzePositiveCycle(ArcIndex first_arc, Trail* trail,
                            std::vector<Literal>* must_be_all_true,
                            std::vector<Literal>* literal_reason,
                            std::vector<IntegerLiteral>* integer_reason);
  void CleanUpMarkedArcsAndParents();

  // Loops over all the arcs and verify that there is no propagation left.
  // This is only meant to be used in a DCHECK() and is not optimized.
  bool NoPropagationLeft(const Trail& trail) const;

  // Update relations_.
  void PushConditionalRelations(const ArcInfo& arc);

  // External class needed to get the IntegerVariable lower bounds and Enqueue
  // new ones.
  EnforcedLinear2Bounds* relations_;
  Trail* trail_;
  IntegerTrail* integer_trail_;
  SharedStatistics* shared_stats_ = nullptr;
  GenericLiteralWatcher* watcher_;
  int watcher_id_;

  // The key to our incrementality. This will be cleared once the propagation
  // is done, and automatically updated by the integer_trail_ with all the
  // IntegerVariable that changed since the last clear.
  SparseBitset<IntegerVariable> modified_vars_;

  // An arc needs to be inspected for propagation (i.e. is impacted) if its
  // tail_var changed. If an arc has 3 variables (tail, offset, head), it will
  // appear as 6 different entries in the arcs_ vector, one for each variable
  // and its negation, each time with a different tail.
  //
  // TODO(user): rearranging the index so that the arc of the same node are
  // consecutive like in StaticGraph should have a big performance impact.
  //
  // TODO(user): We do not need to store ArcInfo.tail_var here.
  util_intops::StrongVector<IntegerVariable, absl::InlinedVector<ArcIndex, 6>>
      impacted_arcs_;
  util_intops::StrongVector<ArcIndex, ArcInfo> arcs_;

  // This is similar to impacted_arcs_/arcs_ but it is only used to propagate
  // one of the presence literals when the arc cannot be present. An arc needs
  // to appear only once in potential_arcs_, but it will be referenced by
  // all its variable in impacted_potential_arcs_.
  util_intops::StrongVector<IntegerVariable,
                            absl::InlinedVector<OptionalArcIndex, 6>>
      impacted_potential_arcs_;
  util_intops::StrongVector<OptionalArcIndex, ArcInfo> potential_arcs_;

  // Each time a literal becomes true, this list the set of arcs for which we
  // need to decrement their count. When an arc count reach zero, it must be
  // added to the set of impacted_arcs_. Note that counts never becomes
  // negative.
  //
  // TODO(user): Try a one-watcher approach instead. Note that in most cases
  // arc should be controlled by 1 or 2 literals, so not sure it is worth it.
  util_intops::StrongVector<LiteralIndex, absl::InlinedVector<ArcIndex, 6>>
      literal_to_new_impacted_arcs_;
  util_intops::StrongVector<ArcIndex, int> arc_counts_;

  // Temp vectors to hold the reason of an assignment.
  std::vector<Literal> literal_reason_;
  std::vector<IntegerLiteral> integer_reason_;

  // Temp vectors for the Bellman-Ford algorithm. The graph in which this
  // algorithm works is in one to one correspondence with the IntegerVariable in
  // impacted_arcs_.
  std::deque<int> bf_queue_;
  std::vector<bool> bf_in_queue_;
  std::vector<bool> bf_can_be_skipped_;
  std::vector<ArcIndex> bf_parent_arc_of_;

  // Temp vector used by the tree traversal in DisassembleSubtree().
  std::vector<int> tmp_vector_;

  // Stats.
  int64_t num_cycles_ = 0;
  int64_t num_pushes_ = 0;
  int64_t num_enforcement_pushes_ = 0;
};

// =============================================================================
// Implementation of the small API functions below.
// =============================================================================

inline void PrecedencesPropagator::AddPrecedence(IntegerVariable i1,
                                                 IntegerVariable i2) {
  AddArc(i1, i2, /*offset=*/IntegerValue(0), /*offset_var=*/kNoIntegerVariable,
         {});
}

inline void PrecedencesPropagator::AddPrecedenceWithOffset(
    IntegerVariable i1, IntegerVariable i2, IntegerValue offset) {
  AddArc(i1, i2, offset, /*offset_var=*/kNoIntegerVariable, {});
}

inline void PrecedencesPropagator::AddConditionalPrecedence(IntegerVariable i1,
                                                            IntegerVariable i2,
                                                            Literal l) {
  AddArc(i1, i2, /*offset=*/IntegerValue(0), /*offset_var=*/kNoIntegerVariable,
         {l});
}

inline void PrecedencesPropagator::AddConditionalPrecedenceWithOffset(
    IntegerVariable i1, IntegerVariable i2, IntegerValue offset, Literal l) {
  AddArc(i1, i2, offset, /*offset_var=*/kNoIntegerVariable, {l});
}

inline void PrecedencesPropagator::AddPrecedenceWithVariableOffset(
    IntegerVariable i1, IntegerVariable i2, IntegerVariable offset_var) {
  AddArc(i1, i2, /*offset=*/IntegerValue(0), offset_var, {});
}

inline void PrecedencesPropagator::AddPrecedenceWithAllOptions(
    IntegerVariable i1, IntegerVariable i2, IntegerValue offset,
    IntegerVariable offset_var, absl::Span<const Literal> presence_literals) {
  AddArc(i1, i2, offset, offset_var, presence_literals);
}

// =============================================================================
// Model based functions.
// =============================================================================

// l => (a + b <= ub).
inline void AddConditionalSum2LowerOrEqual(
    absl::Span<const Literal> enforcement_literals, IntegerVariable a,
    IntegerVariable b, int64_t ub, Model* model) {
  // TODO(user): Refactor to be sure we do not miss any level zero relations.
  if (enforcement_literals.empty()) {
    LinearExpression2 expr(a, b, 1, 1);
    model->GetOrCreate<RootLevelLinear2Bounds>()->AddUpperBound(
        expr, IntegerValue(ub));
  }

  PrecedencesPropagator* p = model->GetOrCreate<PrecedencesPropagator>();
  p->AddPrecedenceWithAllOptions(a, NegationOf(b), IntegerValue(-ub),
                                 kNoIntegerVariable, enforcement_literals);
}

// l => (a + b + c <= ub).
//
// TODO(user): Use level zero bounds to infer binary precedence relations?
inline void AddConditionalSum3LowerOrEqual(
    absl::Span<const Literal> enforcement_literals, IntegerVariable a,
    IntegerVariable b, IntegerVariable c, int64_t ub, Model* model) {
  PrecedencesPropagator* p = model->GetOrCreate<PrecedencesPropagator>();
  p->AddPrecedenceWithAllOptions(a, NegationOf(c), IntegerValue(-ub), b,
                                 enforcement_literals);
}

// a == b.
//
// ABSL_DEPRECATED("Use linear constraint API instead")
inline std::function<void(Model*)> Equality(IntegerVariable a,
                                            IntegerVariable b) {
  return [=](Model* model) {
    auto* precedences = model->GetOrCreate<PrecedencesPropagator>();
    precedences->AddPrecedence(a, b);
    precedences->AddPrecedence(b, a);
  };
}

// is_le => (a + offset <= b).
//
// ABSL_DEPRECATED("Use linear constraint API instead")
inline std::function<void(Model*)> ConditionalLowerOrEqualWithOffset(
    IntegerVariable a, IntegerVariable b, int64_t offset, Literal is_le) {
  return [=](Model* model) {
    PrecedencesPropagator* p = model->GetOrCreate<PrecedencesPropagator>();
    p->AddConditionalPrecedenceWithOffset(a, b, IntegerValue(offset), is_le);
  };
}

}  // namespace sat
}  // namespace operations_research

#endif  // OR_TOOLS_SAT_PRECEDENCES_H_
