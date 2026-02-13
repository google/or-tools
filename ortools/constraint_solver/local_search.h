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

#ifndef ORTOOLS_CONSTRAINT_SOLVER_LOCAL_SEARCH_H_
#define ORTOOLS_CONSTRAINT_SOLVER_LOCAL_SEARCH_H_

#include <algorithm>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include "absl/algorithm/container.h"
#include "absl/container/flat_hash_set.h"
#include "absl/log/check.h"
#include "absl/types/span.h"
#include "ortools/base/base_export.h"
#include "ortools/base/strong_int.h"
#include "ortools/base/strong_vector.h"
#include "ortools/constraint_solver/assignment.h"
#include "ortools/constraint_solver/constraint_solver.h"
#include "ortools/util/bitset.h"

namespace operations_research {

// ----- LocalSearchOperator -----

/// The base class for all local search operators.
///
/// A local search operator is an object that generates neighbors of the current
/// solution.
///
/// It has a Start() method which synchronizes the operator with the
/// current assignment (solution). Then one can iterate over the neighbors using
/// the MakeNextNeighbor method. This method returns an assignment which
/// represents the incremental changes to the current solution. It also returns
/// a second assignment representing the changes to the last solution defined by
/// the neighborhood operator; this assignment is empty if the neighborhood
/// operator cannot track this information.
///
// TODO(user): rename Start to Synchronize ?
// TODO(user): decouple the iterating from the defining of a neighbor.
class LocalSearchOperator : public BaseObject {
 public:
  LocalSearchOperator() {}
  ~LocalSearchOperator() override {}
  virtual bool MakeNextNeighbor(Assignment* delta, Assignment* deltadelta) = 0;
  virtual void EnterSearch() {}
  virtual void Start(const Assignment* assignment) = 0;
  virtual void Reset() {}
#ifndef SWIG
  virtual const LocalSearchOperator* Self() const { return this; }
#endif  // SWIG
  virtual bool HasFragments() const { return false; }
  virtual bool HoldsDelta() const { return false; }
};

class LocalSearchOperatorState {
 public:
  LocalSearchOperatorState() {}

  void SetCurrentDomainInjectiveAndKeepInverseValues(int max_value) {
    max_inversible_index_ = candidate_values_.size();
    candidate_value_to_index_.resize(max_value + 1, -1);
    committed_value_to_index_.resize(max_value + 1, -1);
  }

  /// Returns the value in the current assignment of the variable of given
  /// index.
  int64_t CandidateValue(int64_t index) const {
    DCHECK_LT(index, candidate_values_.size());
    return candidate_values_[index];
  }
  int64_t CommittedValue(int64_t index) const {
    return committed_values_[index];
  }
  int64_t CheckPointValue(int64_t index) const {
    return checkpoint_values_[index];
  }
  void SetCandidateValue(int64_t index, int64_t value) {
    candidate_values_[index] = value;
    if (index < max_inversible_index_) {
      candidate_value_to_index_[value] = index;
    }
    MarkChange(index);
  }

  bool CandidateIsActive(int64_t index) const {
    return candidate_is_active_[index];
  }
  void SetCandidateActive(int64_t index, bool active) {
    if (active) {
      candidate_is_active_.Set(index);
    } else {
      candidate_is_active_.Clear(index);
    }
    MarkChange(index);
  }

  void Commit() {
    for (const int64_t index : changes_.PositionsSetAtLeastOnce()) {
      const int64_t value = candidate_values_[index];
      committed_values_[index] = value;
      if (index < max_inversible_index_) {
        committed_value_to_index_[value] = index;
      }
      committed_is_active_.CopyBucket(candidate_is_active_, index);
    }
    changes_.ResetAllToFalse();
    incremental_changes_.ResetAllToFalse();
  }

  void CheckPoint() { checkpoint_values_ = committed_values_; }

  void Revert(bool only_incremental) {
    incremental_changes_.ResetAllToFalse();
    if (only_incremental) return;

    for (const int64_t index : changes_.PositionsSetAtLeastOnce()) {
      const int64_t committed_value = committed_values_[index];
      candidate_values_[index] = committed_value;
      if (index < max_inversible_index_) {
        candidate_value_to_index_[committed_value] = index;
      }
      candidate_is_active_.CopyBucket(committed_is_active_, index);
    }
    changes_.ResetAllToFalse();
  }

  const std::vector<int64_t>& CandidateIndicesChanged() const {
    return changes_.PositionsSetAtLeastOnce();
  }
  const std::vector<int64_t>& IncrementalIndicesChanged() const {
    return incremental_changes_.PositionsSetAtLeastOnce();
  }

  void Resize(int size) {
    candidate_values_.resize(size);
    committed_values_.resize(size);
    checkpoint_values_.resize(size);
    candidate_is_active_.Resize(size);
    committed_is_active_.Resize(size);
    changes_.ClearAndResize(size);
    incremental_changes_.ClearAndResize(size);
  }

  int64_t CandidateInverseValue(int64_t value) const {
    return candidate_value_to_index_[value];
  }
  int64_t CommittedInverseValue(int64_t value) const {
    return committed_value_to_index_[value];
  }

 private:
  void MarkChange(int64_t index) {
    incremental_changes_.Set(index);
    changes_.Set(index);
  }

  std::vector<int64_t> candidate_values_;
  std::vector<int64_t> committed_values_;
  std::vector<int64_t> checkpoint_values_;

  Bitset64<> candidate_is_active_;
  Bitset64<> committed_is_active_;

  SparseBitset<> changes_;
  SparseBitset<> incremental_changes_;

  int64_t max_inversible_index_ = -1;
  std::vector<int64_t> candidate_value_to_index_;
  std::vector<int64_t> committed_value_to_index_;
};

/// Specialization of LocalSearchOperator built from an array of IntVars
/// which specifies the scope of the operator.
/// This class also takes care of storing current variable values in Start(),
/// keeps track of changes done by the operator and builds the delta.
/// The Deactivate() method can be used to perform Large Neighborhood Search.
class IntVarLocalSearchOperator : public LocalSearchOperator {
 public:
  // If keep_inverse_values is true, assumes that vars models an injective
  // function f with domain [0, vars.size()) in which case the operator will
  // maintain the inverse function.
  explicit IntVarLocalSearchOperator(const std::vector<IntVar*>& vars,
                                     bool keep_inverse_values = false) {
    AddVars(vars);
    if (keep_inverse_values) {
      int64_t max_value = -1;
      for (const IntVar* const var : vars) {
        max_value = std::max(max_value, var->Max());
      }
      state_.SetCurrentDomainInjectiveAndKeepInverseValues(max_value);
    }
  }
  ~IntVarLocalSearchOperator() override {}

  bool HoldsDelta() const override { return true; }
  /// This method should not be overridden. Override OnStart() instead which is
  /// called before exiting this method.
  void Start(const Assignment* assignment) override {
    state_.CheckPoint();
    RevertChanges(false);
    const int size = Size();
    CHECK_LE(size, assignment->Size())
        << "Assignment contains fewer variables than operator";
    const Assignment::IntContainer& container = assignment->IntVarContainer();
    for (int i = 0; i < size; ++i) {
      const IntVarElement* element = &(container.Element(i));
      if (element->Var() != vars_[i]) {
        CHECK(container.Contains(vars_[i]))
            << "Assignment does not contain operator variable " << vars_[i];
        element = &(container.Element(vars_[i]));
      }
      state_.SetCandidateValue(i, element->Value());
      state_.SetCandidateActive(i, element->Activated());
    }
    state_.Commit();
    OnStart();
  }
  virtual bool IsIncremental() const { return false; }

  int Size() const { return vars_.size(); }
  /// Returns the value in the current assignment of the variable of given
  /// index.
  int64_t Value(int64_t index) const {
    DCHECK_LT(index, vars_.size());
    return state_.CandidateValue(index);
  }
  /// Returns the variable of given index.
  IntVar* Var(int64_t index) const { return vars_[index]; }
  virtual bool SkipUnchanged(int) const { return false; }
  int64_t OldValue(int64_t index) const { return state_.CommittedValue(index); }
  int64_t PrevValue(int64_t index) const {
    return state_.CheckPointValue(index);
  }
  void SetValue(int64_t index, int64_t value) {
    state_.SetCandidateValue(index, value);
  }
  bool Activated(int64_t index) const {
    return state_.CandidateIsActive(index);
  }
  void Activate(int64_t index) { state_.SetCandidateActive(index, true); }
  void Deactivate(int64_t index) { state_.SetCandidateActive(index, false); }

  bool ApplyChanges(Assignment* delta, Assignment* deltadelta) const {
    if (IsIncremental() && candidate_has_changes_) {
      for (const int64_t index : state_.IncrementalIndicesChanged()) {
        IntVar* var = Var(index);
        const int64_t value = Value(index);
        const bool activated = Activated(index);
        AddToAssignment(var, value, activated, nullptr, index, deltadelta);
        AddToAssignment(var, value, activated, &assignment_indices_, index,
                        delta);
      }
    } else {
      delta->Clear();
      for (const int64_t index : state_.CandidateIndicesChanged()) {
        const int64_t value = Value(index);
        const bool activated = Activated(index);
        if (!activated || value != OldValue(index) || !SkipUnchanged(index)) {
          AddToAssignment(Var(index), value, activated, &assignment_indices_,
                          index, delta);
        }
      }
    }
    return true;
  }

  void RevertChanges(bool change_was_incremental) {
    candidate_has_changes_ = change_was_incremental && IsIncremental();

    if (!candidate_has_changes_) {
      for (const int64_t index : state_.CandidateIndicesChanged()) {
        assignment_indices_[index] = -1;
      }
    }
    state_.Revert(candidate_has_changes_);
  }

  void AddVars(const std::vector<IntVar*>& vars) {
    if (!vars.empty()) {
      vars_.insert(vars_.end(), vars.begin(), vars.end());
      const int64_t size = Size();
      assignment_indices_.resize(size, -1);
      state_.Resize(size);
    }
  }

  /// Called by Start() after synchronizing the operator with the current
  /// assignment. Should be overridden instead of Start() to avoid calling
  /// IntVarLocalSearchOperator::Start explicitly.
  virtual void OnStart() {}

  /// OnStart() should really be protected, but then SWIG doesn't see it. So we
  /// make it public, but only subclasses should access to it (to override it).

  /// Redefines MakeNextNeighbor to export a simpler interface. The calls to
  /// ApplyChanges() and RevertChanges() are factored in this method, hiding
  /// both delta and deltadelta from subclasses which only need to override
  /// MakeOneNeighbor().
  /// Therefore this method should not be overridden. Override MakeOneNeighbor()
  /// instead.
  bool MakeNextNeighbor(Assignment* delta, Assignment* deltadelta) override;

 protected:
  /// Creates a new neighbor. It returns false when the neighborhood is
  /// completely explored.
  // TODO(user): make it pure virtual, implies porting all apps overriding
  /// MakeNextNeighbor() in a subclass of IntVarLocalSearchOperator.
  virtual bool MakeOneNeighbor();

  int64_t InverseValue(int64_t index) const {
    return state_.CandidateInverseValue(index);
  }
  int64_t OldInverseValue(int64_t index) const {
    return state_.CommittedInverseValue(index);
  }

  void AddToAssignment(IntVar* var, int64_t value, bool active,
                       std::vector<int>* assignment_indices, int64_t index,
                       Assignment* assignment) const {
    Assignment::IntContainer* const container =
        assignment->MutableIntVarContainer();
    IntVarElement* element = nullptr;
    if (assignment_indices != nullptr) {
      if ((*assignment_indices)[index] == -1) {
        (*assignment_indices)[index] = container->Size();
        element = assignment->FastAdd(var);
      } else {
        element = container->MutableElement((*assignment_indices)[index]);
      }
    } else {
      element = assignment->FastAdd(var);
    }
    if (active) {
      element->SetValue(value);
      element->Activate();
    } else {
      element->Deactivate();
    }
  }

 private:
  std::vector<IntVar*> vars_;
  mutable std::vector<int> assignment_indices_;
  bool candidate_has_changes_ = false;

  LocalSearchOperatorState state_;
};

/// This is the base class for building an Lns operator. An Lns fragment is a
/// collection of variables which will be relaxed. Fragments are built with
/// NextFragment(), which returns false if there are no more fragments to build.
/// Optionally one can override InitFragments, which is called from
/// LocalSearchOperator::Start to initialize fragment data.
///
/// Here's a sample relaxing one variable at a time:
///
/// class OneVarLns : public BaseLns {
///  public:
///   OneVarLns(const std::vector<IntVar*>& vars) : BaseLns(vars), index_(0) {}
///   virtual ~OneVarLns() {}
///   virtual void InitFragments() { index_ = 0; }
///   virtual bool NextFragment() {
///     const int size = Size();
///     if (index_ < size) {
///       AppendToFragment(index_);
///       ++index_;
///       return true;
///     } else {
///       return false;
///     }
///   }
///
///  private:
///   int index_;
/// };
class BaseLns : public IntVarLocalSearchOperator {
 public:
  explicit BaseLns(const std::vector<IntVar*>& vars);
  ~BaseLns() override;
  virtual void InitFragments();
  virtual bool NextFragment() = 0;
  void AppendToFragment(int index);
  int FragmentSize() const;
  bool HasFragments() const override { return true; }

 protected:
  /// This method should not be overridden. Override NextFragment() instead.
  bool MakeOneNeighbor() override;

 private:
  /// This method should not be overridden. Override InitFragments() instead.
  void OnStart() override;
  std::vector<int> fragment_;
};

/// Defines operators which change the value of variables;
/// each neighbor corresponds to *one* modified variable.
/// Sub-classes have to define ModifyValue which determines what the new
/// variable value is going to be (given the current value and the variable).
class ChangeValue : public IntVarLocalSearchOperator {
 public:
  explicit ChangeValue(const std::vector<IntVar*>& vars);
  ~ChangeValue() override;
  virtual int64_t ModifyValue(int64_t index, int64_t value) = 0;

 protected:
  /// This method should not be overridden. Override ModifyValue() instead.
  bool MakeOneNeighbor() override;

 private:
  void OnStart() override;

  int index_;
};

// Iterators on nodes used by Pathoperator to traverse the search space.

class AlternativeNodeIterator {
 public:
  explicit AlternativeNodeIterator(bool use_sibling)
      : use_sibling_(use_sibling) {}
  ~AlternativeNodeIterator() {}
  template <class PathOperator>
  void Reset(const PathOperator& path_operator, int base_index_reference) {
    index_ = 0;
    DCHECK(path_operator.ConsiderAlternatives(base_index_reference));
    const int64_t base_node = path_operator.BaseNode(base_index_reference);
    const int alternative_index =
        use_sibling_ ? path_operator.GetSiblingAlternativeIndex(base_node)
                     : path_operator.GetAlternativeIndex(base_node);
    alternative_set_ =
        alternative_index >= 0
            ? absl::Span<const int64_t>(
                  path_operator.alternative_sets_[alternative_index])
            : absl::Span<const int64_t>();
  }
  bool Next() { return ++index_ < alternative_set_.size(); }
  int GetValue() const {
    return (index_ >= alternative_set_.size()) ? -1 : alternative_set_[index_];
  }

 private:
  const bool use_sibling_;
  int index_ = 0;
  absl::Span<const int64_t> alternative_set_;
};

class NodeNeighborIterator {
 public:
  NodeNeighborIterator() {}
  ~NodeNeighborIterator() {}
  template <class PathOperator>
  void Reset(const PathOperator& path_operator, int base_index_reference) {
    using Span = absl::Span<const int>;
    index_ = 0;
    const int64_t base_node = path_operator.BaseNode(base_index_reference);
    const int64_t start_node = path_operator.StartNode(base_index_reference);
    const auto& get_incoming_neighbors =
        path_operator.iteration_parameters_.get_incoming_neighbors;
    incoming_neighbors_ =
        path_operator.IsPathStart(base_node) || !get_incoming_neighbors
            ? Span()
            : Span(get_incoming_neighbors(base_node, start_node));
    const auto& get_outgoing_neighbors =
        path_operator.iteration_parameters_.get_outgoing_neighbors;
    outgoing_neighbors_ =
        path_operator.IsPathEnd(base_node) || !get_outgoing_neighbors
            ? Span()
            : Span(get_outgoing_neighbors(base_node, start_node));
  }
  bool Next() {
    return ++index_ < incoming_neighbors_.size() + outgoing_neighbors_.size();
  }
  int GetValue() const {
    if (index_ < incoming_neighbors_.size()) {
      return incoming_neighbors_[index_];
    }
    const int index = index_ - incoming_neighbors_.size();
    return (index >= outgoing_neighbors_.size()) ? -1
                                                 : outgoing_neighbors_[index];
  }
  bool IsIncomingNeighbor() const {
    return index_ < incoming_neighbors_.size();
  }
  bool IsOutgoingNeighbor() const {
    return index_ >= incoming_neighbors_.size();
  }

 private:
  int index_ = 0;
  absl::Span<const int> incoming_neighbors_;
  absl::Span<const int> outgoing_neighbors_;
};

template <class PathOperator>
class BaseNodeIterators {
 public:
  BaseNodeIterators(const PathOperator* path_operator, int base_index_reference)
      : path_operator_(*path_operator),
        base_index_reference_(base_index_reference) {}
  AlternativeNodeIterator* GetSiblingAlternativeIterator() const {
    DCHECK(!alternatives_.empty());
    DCHECK(!finished_);
    return alternatives_[0].get();
  }
  AlternativeNodeIterator* GetAlternativeIterator() const {
    DCHECK(!alternatives_.empty());
    DCHECK(!finished_);
    return alternatives_[1].get();
  }
  NodeNeighborIterator* GetNeighborIterator() const {
    DCHECK(neighbors_);
    DCHECK(!finished_);
    return neighbors_.get();
  }
  void Initialize() {
    if (path_operator_.ConsiderAlternatives(base_index_reference_)) {
      alternatives_.push_back(std::make_unique<AlternativeNodeIterator>(
          /*is_sibling=*/true));
      alternatives_.push_back(std::make_unique<AlternativeNodeIterator>(
          /*is_sibling=*/false));
    }
    if (path_operator_.HasNeighbors()) {
      neighbors_ = std::make_unique<NodeNeighborIterator>();
    }
  }
  void Reset(bool update_end_nodes = false) {
    finished_ = false;
    for (auto& alternative_iterator : alternatives_) {
      alternative_iterator->Reset(path_operator_, base_index_reference_);
    }
    if (neighbors_) {
      neighbors_->Reset(path_operator_, base_index_reference_);
      if (update_end_nodes) neighbor_end_node_ = neighbors_->GetValue();
    }
  }
  bool Increment() {
    DCHECK(!finished_);
    for (auto& alternative_iterator : alternatives_) {
      if (alternative_iterator->Next()) return true;
      alternative_iterator->Reset(path_operator_, base_index_reference_);
    }
    if (neighbors_) {
      if (neighbors_->Next()) return true;
      neighbors_->Reset(path_operator_, base_index_reference_);
    }
    finished_ = true;
    return false;
  }
  bool HasReachedEnd() const {
    // TODO(user): Extend to other iterators.
    if (!neighbors_) return true;
    return neighbor_end_node_ == neighbors_->GetValue();
  }

 private:
  const PathOperator& path_operator_;
  int base_index_reference_ = -1;
#ifndef SWIG
  std::vector<std::unique_ptr<AlternativeNodeIterator>> alternatives_;
#endif  // SWIG
  std::unique_ptr<NodeNeighborIterator> neighbors_;
  int neighbor_end_node_ = -1;
  bool finished_ = false;
};

/// Base class of the local search operators dedicated to path modifications
/// (a path is a set of nodes linked together by arcs).
/// This family of neighborhoods supposes they are handling next variables
/// representing the arcs (var[i] represents the node immediately after i on
/// a path).
/// Several services are provided:
/// - arc manipulators (SetNext(), ReverseChain(), MoveChain())
/// - path inspectors (Next(), Prev(), IsPathEnd())
/// - path iterators: operators need a given number of nodes to define a
///   neighbor; this class provides the iteration on a given number of (base)
///   nodes which can be used to define a neighbor (through the BaseNode method)
/// Subclasses only need to override MakeNeighbor to create neighbors using
/// the services above (no direct manipulation of assignments).
template <bool ignore_path_vars>
class PathOperator : public IntVarLocalSearchOperator {
 public:
  /// Set of parameters used to configure how the neighborhood is traversed.
  struct IterationParameters {
    /// Number of nodes needed to define a neighbor.
    int number_of_base_nodes;
    /// Skip paths which have been proven locally optimal. Note this might skip
    /// neighbors when paths are not independent.
    bool skip_locally_optimal_paths;
    /// True if path ends should be considered when iterating over neighbors.
    bool accept_path_end_base;
    /// Callback returning an index such that if
    /// c1 = start_empty_path_class(StartNode(p1)),
    /// c2 = start_empty_path_class(StartNode(p2)),
    /// p1 and p2 are path indices,
    /// then if c1 == c2, p1 and p2 are equivalent if they are empty.
    /// This is used to remove neighborhood symmetries on equivalent empty
    /// paths; for instance if a node cannot be moved to an empty path, then all
    /// moves moving the same node to equivalent empty paths will be skipped.
    /// 'start_empty_path_class' can be nullptr in which case no symmetries will
    /// be removed.
    std::function<int(int64_t)> start_empty_path_class;
    /// Callbacks returning incoming/outgoing neighbors of a node on a path
    /// starting at start_node.
    std::function<const std::vector<int>&(
        /*node=*/int, /*start_node=*/int)>
        get_incoming_neighbors;
    std::function<const std::vector<int>&(
        /*node=*/int, /*start_node=*/int)>
        get_outgoing_neighbors;
  };
  /// Builds an instance of PathOperator from next and path variables.
  PathOperator(const std::vector<IntVar*>& next_vars,
               const std::vector<IntVar*>& path_vars,
               IterationParameters iteration_parameters)
      : IntVarLocalSearchOperator(next_vars, true),
        number_of_nexts_(next_vars.size()),
        base_nodes_(
            std::make_unique<int[]>(iteration_parameters.number_of_base_nodes)),
        end_nodes_(
            std::make_unique<int[]>(iteration_parameters.number_of_base_nodes)),
        base_paths_(
            std::make_unique<int[]>(iteration_parameters.number_of_base_nodes)),
        node_path_starts_(number_of_nexts_, -1),
        node_path_ends_(number_of_nexts_, -1),
        just_started_(false),
        first_start_(true),
        next_base_to_increment_(iteration_parameters.number_of_base_nodes),
        iteration_parameters_(std::move(iteration_parameters)),
        optimal_paths_enabled_(false),
        active_paths_(number_of_nexts_),
        alternative_index_(next_vars.size(), -1) {
    DCHECK_GT(iteration_parameters_.number_of_base_nodes, 0);
    for (int i = 0; i < iteration_parameters_.number_of_base_nodes; ++i) {
      base_node_iterators_.push_back(BaseNodeIterators<PathOperator>(this, i));
    }
    if constexpr (!ignore_path_vars) {
      AddVars(path_vars);
    }
    path_basis_.push_back(0);
    for (int i = 1; i < iteration_parameters_.number_of_base_nodes; ++i) {
      if (!OnSamePathAsPreviousBase(i)) path_basis_.push_back(i);
    }
    if ((path_basis_.size() > 2) ||
        (!next_vars.empty() && !next_vars.back()
                                    ->solver()
                                    ->parameters()
                                    .skip_locally_optimal_paths())) {
      iteration_parameters_.skip_locally_optimal_paths = false;
    }
  }
  PathOperator(
      const std::vector<IntVar*>& next_vars,
      const std::vector<IntVar*>& path_vars, int number_of_base_nodes,
      bool skip_locally_optimal_paths, bool accept_path_end_base,
      std::function<int(int64_t)> start_empty_path_class,
      std::function<const std::vector<int>&(int, int)> get_incoming_neighbors,
      std::function<const std::vector<int>&(int, int)> get_outgoing_neighbors)
      : PathOperator(next_vars, path_vars,
                     {number_of_base_nodes, skip_locally_optimal_paths,
                      accept_path_end_base, std::move(start_empty_path_class),
                      std::move(get_incoming_neighbors),
                      std::move(get_outgoing_neighbors)}) {}
  ~PathOperator() override {}
  virtual bool MakeNeighbor() = 0;
  void EnterSearch() override {
    first_start_ = true;
    ResetIncrementalism();
  }
  void Reset() override {
    active_paths_.Clear();
    ResetIncrementalism();
  }

  // TODO(user): Make the following methods protected.
  bool SkipUnchanged(int index) const override {
    if constexpr (ignore_path_vars) return true;
    if (index < number_of_nexts_) {
      const int path_index = index + number_of_nexts_;
      return Value(path_index) == OldValue(path_index);
    }
    const int next_index = index - number_of_nexts_;
    return Value(next_index) == OldValue(next_index);
  }

  /// Returns the next node in the current delta.
  int64_t Next(int64_t node) const {
    DCHECK(!IsPathEnd(node));
    return Value(node);
  }

  /// Returns the node before node in the current delta.
  int64_t Prev(int64_t node) const {
    DCHECK(!IsPathStart(node));
    DCHECK_EQ(Next(InverseValue(node)), node);
    return InverseValue(node);
  }

  /// Returns the index of the path to which node belongs in the current delta.
  /// Only returns a valid value if path variables are taken into account.
  int64_t Path(int64_t node) const {
    if constexpr (ignore_path_vars) return 0LL;
    return Value(node + number_of_nexts_);
  }

  /// Number of next variables.
  int number_of_nexts() const { return number_of_nexts_; }

 protected:
  /// This method should not be overridden. Override MakeNeighbor() instead.
  bool MakeOneNeighbor() override {
    while (IncrementPosition()) {
      // Need to revert changes here since MakeNeighbor might have returned
      // false and have done changes in the previous iteration.
      RevertChanges(true);
      if (MakeNeighbor()) {
        return true;
      }
    }
    return false;
  }

  /// Called by OnStart() after initializing node information. Should be
  /// overridden instead of OnStart() to avoid calling PathOperator::OnStart
  /// explicitly.
  virtual void OnNodeInitialization() {}
  /// When entering a new search or using metaheuristics, path operators
  /// reactivate optimal routes and iterating re-starts at route starts, which
  /// can potentially be out of sync with the last incremental moves.
  /// This requires resetting incrementalism.
  virtual void ResetIncrementalism() {}

  /// Returns the ith base node of the operator.
  int64_t BaseNode(int i) const { return base_nodes_[i]; }
  /// Returns the alternative node for the ith base node.
  int64_t BaseAlternativeNode(int i) const {
    return GetNodeWithDefault(base_node_iterators_[i].GetAlternativeIterator(),
                              BaseNode(i));
  }
  /// Returns the alternative node for the sibling of the ith base node.
  int64_t BaseSiblingAlternativeNode(int i) const {
    return GetNodeWithDefault(
        base_node_iterators_[i].GetSiblingAlternativeIterator(), BaseNode(i));
  }
  /// Returns the start node of the ith base node.
  int64_t StartNode(int i) const { return path_starts_[base_paths_[i]]; }
  /// Returns the end node of the ith base node.
  int64_t EndNode(int i) const { return path_ends_[base_paths_[i]]; }
  /// Returns the vector of path start nodes.
  const std::vector<int64_t>& path_starts() const { return path_starts_; }
  /// Returns the class of the path of the ith base node.
  int PathClass(int i) const { return PathClassFromStartNode(StartNode(i)); }
  int PathClassFromStartNode(int64_t start_node) const {
    return iteration_parameters_.start_empty_path_class != nullptr
               ? iteration_parameters_.start_empty_path_class(start_node)
               : start_node;
  }

  /// When the operator is being synchronized with a new solution (when Start()
  /// is called), returns true to restart the exploration of the neighborhood
  /// from the start of the last paths explored; returns false to restart the
  /// exploration at the last nodes visited.
  /// This is used to avoid restarting on base nodes which have changed paths,
  /// leading to potentially skipping neighbors.
  // TODO(user): remove this when automatic detection of such cases in done.
  virtual bool RestartAtPathStartOnSynchronize() { return false; }
  /// Returns true if a base node has to be on the same path as the "previous"
  /// base node (base node of index base_index - 1).
  /// Useful to limit neighborhood exploration to nodes on the same path.
  // TODO(user): ideally this should be OnSamePath(int64_t node1, int64_t
  // node2);
  /// it's currently way more complicated to implement.
  virtual bool OnSamePathAsPreviousBase(int64_t) { return false; }
  /// Returns the index of the node to which the base node of index base_index
  /// must be set to when it reaches the end of a path.
  /// By default, it is set to the start of the current path.
  /// When this method is called, one can only assume that base nodes with
  /// indices < base_index have their final position.
  virtual int64_t GetBaseNodeRestartPosition(int base_index) {
    return StartNode(base_index);
  }
  /// Set the next base to increment on next iteration. All base > base_index
  /// will be reset to their start value.
  virtual void SetNextBaseToIncrement(int64_t base_index) {
    next_base_to_increment_ = base_index;
  }
  /// Indicates if alternatives should be considered when iterating over base
  /// nodes.
  virtual bool ConsiderAlternatives(int64_t) const { return false; }

  int64_t OldNext(int64_t node) const {
    DCHECK(!IsPathEnd(node));
    return OldValue(node);
  }

  int64_t PrevNext(int64_t node) const {
    DCHECK(!IsPathEnd(node));
    return PrevValue(node);
  }

  int64_t OldPrev(int64_t node) const {
    DCHECK(!IsPathStart(node));
    return OldInverseValue(node);
  }

  int64_t OldPath(int64_t node) const {
    if constexpr (ignore_path_vars) return 0LL;
    return OldValue(node + number_of_nexts_);
  }

  int CurrentNodePathStart(int64_t node) const {
    return node_path_starts_[node];
  }

  int CurrentNodePathEnd(int64_t node) const { return node_path_ends_[node]; }

  /// Moves the chain starting after the node before_chain and ending at the
  /// node chain_end after the node destination
  bool MoveChain(int64_t before_chain, int64_t chain_end, int64_t destination) {
    if (destination == before_chain || destination == chain_end) return false;
    DCHECK(CheckChainValidity(before_chain, chain_end, destination) &&
           !IsPathEnd(chain_end) && !IsPathEnd(destination));
    const int64_t destination_path = Path(destination);
    const int64_t after_chain = Next(chain_end);
    SetNext(chain_end, Next(destination), destination_path);
    if constexpr (!ignore_path_vars) {
      int current = destination;
      int next = Next(before_chain);
      while (current != chain_end) {
        SetNext(current, next, destination_path);
        current = next;
        next = Next(next);
      }
    } else {
      SetNext(destination, Next(before_chain), destination_path);
    }
    SetNext(before_chain, after_chain, Path(before_chain));
    return true;
  }

  /// Reverses the chain starting after before_chain and ending before
  /// after_chain
  bool ReverseChain(int64_t before_chain, int64_t after_chain,
                    int64_t* chain_last) {
    if (CheckChainValidity(before_chain, after_chain, -1)) {
      int64_t path = Path(before_chain);
      int64_t current = Next(before_chain);
      if (current == after_chain) {
        return false;
      }
      int64_t current_next = Next(current);
      SetNext(current, after_chain, path);
      while (current_next != after_chain) {
        const int64_t next = Next(current_next);
        SetNext(current_next, current, path);
        current = current_next;
        current_next = next;
      }
      SetNext(before_chain, current, path);
      *chain_last = current;
      return true;
    }
    return false;
  }

  /// Returns the last node of the chain of length 'chain_length' starting after
  /// 'before_chain'. Returns nullopt if the chain contains 'exclude' or
  /// reaches the end of the path. Returns 'before_chain' if 'chain_length' is
  /// 0.
  std::optional<int64_t> GetChainEnd(int64_t before_chain, int64_t exclude,
                                     int64_t chain_length) const {
    for (int64_t chain_end = before_chain;
         !IsPathEnd(chain_end) && chain_end != exclude;
         chain_end = Next(chain_end)) {
      if (chain_length-- == 0) return chain_end;
    }
    return std::nullopt;
  }

  /// Swaps the nodes node1 and node2.
  bool SwapNodes(int64_t node1, int64_t node2) {
    if (IsPathEnd(node1) || IsPathEnd(node2) || IsPathStart(node1) ||
        IsPathStart(node2)) {
      return false;
    }
    if (node1 == node2) return false;
    const int64_t prev_node1 = Prev(node1);
    const bool ok = MoveChain(prev_node1, node1, Prev(node2));
    return MoveChain(Prev(node2), node2, prev_node1) || ok;
  }

  /// Insert the inactive node after destination.
  bool MakeActive(int64_t node, int64_t destination) {
    if (IsPathEnd(destination)) return false;
    const int64_t destination_path = Path(destination);
    SetNext(node, Next(destination), destination_path);
    SetNext(destination, node, destination_path);
    return true;
  }
  /// Makes the nodes on the chain starting after before_chain and ending at
  /// chain_end inactive.
  bool MakeChainInactive(int64_t before_chain, int64_t chain_end) {
    const int64_t kNoPath = -1;
    if (CheckChainValidity(before_chain, chain_end, -1) &&
        !IsPathEnd(chain_end)) {
      const int64_t after_chain = Next(chain_end);
      int64_t current = Next(before_chain);
      while (current != after_chain) {
        const int64_t next = Next(current);
        SetNext(current, current, kNoPath);
        current = next;
      }
      SetNext(before_chain, after_chain, Path(before_chain));
      return true;
    }
    return false;
  }

  /// Replaces active by inactive in the current path, making active inactive.
  bool SwapActiveAndInactive(int64_t active, int64_t inactive) {
    if (active == inactive) return false;
    const int64_t prev = Prev(active);
    return MakeChainInactive(prev, active) && MakeActive(inactive, prev);
  }
  /// Swaps both chains, making nodes on active_chain inactive and inserting
  /// active_chain at the position where inactive_chain was.
  bool SwapActiveAndInactiveChains(absl::Span<const int64_t> active_chain,
                                   absl::Span<const int64_t> inactive_chain) {
    if (active_chain.empty()) return false;
    if (active_chain == inactive_chain) return false;
    const int before_active_chain = Prev(active_chain.front());
    if (!MakeChainInactive(before_active_chain, active_chain.back())) {
      return false;
    }
    for (auto it = inactive_chain.crbegin(); it != inactive_chain.crend();
         ++it) {
      if (!MakeActive(*it, before_active_chain)) return false;
    }
    return true;
  }

  /// Sets 'to' to be the node after 'from' on the given path.
  void SetNext(int64_t from, int64_t to, int64_t path) {
    DCHECK_LT(from, number_of_nexts_);
    SetValue(from, to);
    if constexpr (!ignore_path_vars) {
      DCHECK_LT(from + number_of_nexts_, Size());
      SetValue(from + number_of_nexts_, path);
    }
  }

  /// Returns true if node is the last node on the path; defined by the fact
  /// that node is outside the range of the variable array.
  bool IsPathEnd(int64_t node) const { return node >= number_of_nexts_; }

  /// Returns true if node is the first node on the path.
  bool IsPathStart(int64_t node) const { return OldInverseValue(node) == -1; }

  /// Returns true if node is inactive.
  bool IsInactive(int64_t node) const {
    return !IsPathEnd(node) && inactives_[node];
  }

  /// Returns true if the operator needs to restart its initial position at each
  /// call to Start()
  virtual bool InitPosition() const { return false; }
  /// Reset the position of the operator to its position when Start() was last
  /// called; this can be used to let an operator iterate more than once over
  /// the paths.
  void ResetPosition() { just_started_ = true; }

  /// Handling node alternatives.
  /// Adds a set of node alternatives to the neighborhood. No node can be in
  /// two alternatives.
  int AddAlternativeSet(const std::vector<int64_t>& alternative_set) {
    const int alternative = alternative_sets_.size();
    for (int64_t node : alternative_set) {
      DCHECK_EQ(-1, alternative_index_[node]);
      alternative_index_[node] = alternative;
    }
    alternative_sets_.push_back(alternative_set);
    sibling_alternative_.push_back(-1);
    return alternative;
  }
#ifndef SWIG
  /// Adds all sets of node alternatives of a vector of alternative pairs. No
  /// node can be in two alternatives.
  template <typename PairType>
  void AddPairAlternativeSets(
      const std::vector<PairType>& pair_alternative_sets) {
    for (const auto& [alternative_set, sibling_alternative_set] :
         pair_alternative_sets) {
      sibling_alternative_.back() = AddAlternativeSet(alternative_set) + 1;
      AddAlternativeSet(sibling_alternative_set);
    }
  }
#endif  // SWIG
  /// Returns the active node in the given alternative set.
  int64_t GetActiveInAlternativeSet(int alternative_index) const {
    return alternative_index >= 0
               ? active_in_alternative_set_[alternative_index]
               : -1;
  }
  /// Returns the active node in the alternative set of the given node.
  int64_t GetActiveAlternativeNode(int node) const {
    return GetActiveInAlternativeSet(alternative_index_[node]);
  }
  /// Returns the index of the alternative set of the sibling of node.
  int GetSiblingAlternativeIndex(int node) const {
    const int alternative = GetAlternativeIndex(node);
    return alternative >= 0 ? sibling_alternative_[alternative] : -1;
  }
  /// Returns the index of the alternative set of the node.
  int GetAlternativeIndex(int node) const {
    return (node >= alternative_index_.size()) ? -1 : alternative_index_[node];
  }
  /// Returns the active node in the alternative set of the sibling of the given
  /// node.
  int64_t GetActiveAlternativeSibling(int node) const {
    return GetActiveInAlternativeSet(GetSiblingAlternativeIndex(node));
  }
  /// Returns true if the chain is a valid path without cycles from before_chain
  /// to chain_end and does not contain exclude.
  /// In particular, rejects a chain if chain_end is not strictly after
  /// before_chain on the path.
  /// Cycles are detected through chain length overflow.
  bool CheckChainValidity(int64_t before_chain, int64_t chain_end,
                          int64_t exclude) const {
    if (before_chain == chain_end || before_chain == exclude) return false;
    int64_t current = before_chain;
    int chain_size = 0;
    while (current != chain_end) {
      if (chain_size > number_of_nexts_) return false;
      if (IsPathEnd(current)) return false;
      current = Next(current);
      ++chain_size;
      if (current == exclude) return false;
    }
    return true;
  }

  bool HasNeighbors() const {
    return iteration_parameters_.get_incoming_neighbors != nullptr ||
           iteration_parameters_.get_outgoing_neighbors != nullptr;
  }

  struct Neighbor {
    // Index of the neighbor node.
    int neighbor;
    // True if 'neighbor' is an outgoing neighbor (i.e. arc main_node->neighbor)
    // and false if it's an incoming one (arc neighbor->main_node).
    bool outgoing;
  };
  Neighbor GetNeighborForBaseNode(int64_t base_index) const {
    auto* iterator = base_node_iterators_[base_index].GetNeighborIterator();
    return {.neighbor = iterator->GetValue(),
            .outgoing = iterator->IsOutgoingNeighbor()};
  }

  const int number_of_nexts_;

 private:
  template <class NodeIterator>
  static int GetNodeWithDefault(const NodeIterator* node_iterator,
                                int default_value) {
    const int node = node_iterator->GetValue();
    return node >= 0 ? node : default_value;
  }

  void OnStart() override {
    optimal_paths_enabled_ = false;
    if (!iterators_initialized_) {
      iterators_initialized_ = true;
      for (int i = 0; i < iteration_parameters_.number_of_base_nodes; ++i) {
        base_node_iterators_[i].Initialize();
      }
    }
    InitializeBaseNodes();
    InitializeAlternatives();
    OnNodeInitialization();
  }

  /// Returns true if two nodes are on the same path in the current assignment.
  bool OnSamePath(int64_t node1, int64_t node2) const {
    if (IsInactive(node1) != IsInactive(node2)) {
      return false;
    }
    for (int node = node1; !IsPathEnd(node); node = OldNext(node)) {
      if (node == node2) {
        return true;
      }
    }
    for (int node = node2; !IsPathEnd(node); node = OldNext(node)) {
      if (node == node1) {
        return true;
      }
    }
    return false;
  }

  bool CheckEnds() const {
    for (int i = iteration_parameters_.number_of_base_nodes - 1; i >= 0; --i) {
      if (base_nodes_[i] != end_nodes_[i] ||
          !base_node_iterators_[i].HasReachedEnd()) {
        return true;
      }
    }
    return false;
  }
  bool IncrementPosition() {
    const int base_node_size = iteration_parameters_.number_of_base_nodes;

    if (just_started_) {
      just_started_ = false;
      return true;
    }
    const int number_of_paths = path_starts_.size();
    // Finding next base node positions.
    // Increment the position of inner base nodes first (higher index nodes);
    // if a base node is at the end of a path, reposition it at the start
    // of the path and increment the position of the preceding base node (this
    // action is called a restart).
    int last_restarted = base_node_size;
    for (int i = base_node_size - 1; i >= 0; --i) {
      if (base_nodes_[i] < number_of_nexts_ && i <= next_base_to_increment_) {
        if (base_node_iterators_[i].Increment()) break;
        base_nodes_[i] = OldNext(base_nodes_[i]);
        base_node_iterators_[i].Reset();
        if (iteration_parameters_.accept_path_end_base ||
            !IsPathEnd(base_nodes_[i])) {
          break;
        }
      }
      base_nodes_[i] = StartNode(i);
      base_node_iterators_[i].Reset();
      last_restarted = i;
    }
    next_base_to_increment_ = base_node_size;
    // At the end of the loop, base nodes with indexes in
    // [last_restarted, base_node_size[ have been restarted.
    // Restarted base nodes are then repositioned by the virtual
    // GetBaseNodeRestartPosition to reflect position constraints between
    // base nodes (by default GetBaseNodeRestartPosition leaves the nodes
    // at the start of the path).
    // Base nodes are repositioned in ascending order to ensure that all
    // base nodes "below" the node being repositioned have their final
    // position.
    for (int i = last_restarted; i < base_node_size; ++i) {
      base_nodes_[i] = GetBaseNodeRestartPosition(i);
      base_node_iterators_[i].Reset();
    }
    if (last_restarted > 0) {
      return CheckEnds();
    }
    // If all base nodes have been restarted, base nodes are moved to new paths.
    // First we mark the current paths as locally optimal if they have been
    // completely explored.
    if (optimal_paths_enabled_ &&
        iteration_parameters_.skip_locally_optimal_paths) {
      if (path_basis_.size() > 1) {
        for (int i = 1; i < path_basis_.size(); ++i) {
          active_paths_.DeactivatePathPair(StartNode(path_basis_[i - 1]),
                                           StartNode(path_basis_[i]));
        }
      } else {
        active_paths_.DeactivatePathPair(StartNode(path_basis_[0]),
                                         StartNode(path_basis_[0]));
      }
    }
    std::vector<int> current_starts(base_node_size);
    for (int i = 0; i < base_node_size; ++i) {
      current_starts[i] = StartNode(i);
    }
    // Exploration of next paths can lead to locally optimal paths since we are
    // exploring them from scratch.
    optimal_paths_enabled_ = true;
    while (true) {
      for (int i = base_node_size - 1; i >= 0; --i) {
        const int next_path_index = base_paths_[i] + 1;
        if (next_path_index < number_of_paths) {
          base_paths_[i] = next_path_index;
          base_nodes_[i] = path_starts_[next_path_index];
          base_node_iterators_[i].Reset();
          if (i == 0 || !OnSamePathAsPreviousBase(i)) {
            break;
          }
        } else {
          base_paths_[i] = 0;
          base_nodes_[i] = path_starts_[0];
          base_node_iterators_[i].Reset();
        }
      }
      if (!iteration_parameters_.skip_locally_optimal_paths) return CheckEnds();
      // If the new paths have already been completely explored, we can
      // skip them from now on.
      if (path_basis_.size() > 1) {
        for (int j = 1; j < path_basis_.size(); ++j) {
          if (active_paths_.IsPathPairActive(StartNode(path_basis_[j - 1]),
                                             StartNode(path_basis_[j]))) {
            return CheckEnds();
          }
        }
      } else {
        if (active_paths_.IsPathPairActive(StartNode(path_basis_[0]),
                                           StartNode(path_basis_[0]))) {
          return CheckEnds();
        }
      }
      // If we are back to paths we just iterated on or have reached the end
      // of the neighborhood search space, we can stop.
      if (!CheckEnds()) return false;
      bool stop = true;
      for (int i = 0; i < base_node_size; ++i) {
        if (StartNode(i) != current_starts[i]) {
          stop = false;
          break;
        }
      }
      if (stop) return false;
    }
    return CheckEnds();
  }

  void InitializePathStarts() {
    // Detect nodes which do not have any possible predecessor in a path; these
    // nodes are path starts.
    int max_next = -1;
    std::vector<bool> has_prevs(number_of_nexts_, false);
    for (int i = 0; i < number_of_nexts_; ++i) {
      const int next = OldNext(i);
      if (next < number_of_nexts_) {
        has_prevs[next] = true;
      }
      max_next = std::max(max_next, next);
    }
    // Update locally optimal paths.
    if (iteration_parameters_.skip_locally_optimal_paths) {
      active_paths_.Initialize(
          /*is_start=*/[&has_prevs](int node) { return !has_prevs[node]; });
      for (int i = 0; i < number_of_nexts_; ++i) {
        if (!has_prevs[i]) {
          int current = i;
          while (!IsPathEnd(current)) {
            if ((OldNext(current) != PrevNext(current))) {
              active_paths_.ActivatePath(i);
              break;
            }
            current = OldNext(current);
          }
        }
      }
    }
    // Create a list of path starts, dropping equivalent path starts of
    // currently empty paths.
    std::vector<bool> empty_found(number_of_nexts_, false);
    std::vector<int64_t> new_path_starts;
    for (int i = 0; i < number_of_nexts_; ++i) {
      if (!has_prevs[i]) {
        if (IsPathEnd(OldNext(i))) {
          if (iteration_parameters_.start_empty_path_class != nullptr) {
            if (empty_found[iteration_parameters_.start_empty_path_class(i)])
              continue;
            empty_found[iteration_parameters_.start_empty_path_class(i)] = true;
          }
        }
        new_path_starts.push_back(i);
      }
    }
    if (!first_start_) {
      // Synchronizing base_paths_ with base node positions. When the last move
      // was performed a base node could have been moved to a new route in which
      // case base_paths_ needs to be updated. This needs to be done on the path
      // starts before we re-adjust base nodes for new path starts.
      std::vector<int> node_paths(max_next + 1, -1);
      for (int i = 0; i < path_starts_.size(); ++i) {
        int node = path_starts_[i];
        while (!IsPathEnd(node)) {
          node_paths[node] = i;
          node = OldNext(node);
        }
        node_paths[node] = i;
      }
      for (int j = 0; j < iteration_parameters_.number_of_base_nodes; ++j) {
        if (IsInactive(base_nodes_[j]) || node_paths[base_nodes_[j]] == -1) {
          // Base node was made inactive or was moved to a new path, reposition
          // the base node to its restart position.
          base_nodes_[j] = GetBaseNodeRestartPosition(j);
          base_paths_[j] = node_paths[base_nodes_[j]];
        } else {
          base_paths_[j] = node_paths[base_nodes_[j]];
        }
        // Always restart from first alternative.
        base_node_iterators_[j].Reset();
      }
      // Re-adjust current base_nodes and base_paths to take into account new
      // path starts (there could be fewer if a new path was made empty, or more
      // if nodes were added to a formerly empty path).
      int new_index = 0;
      absl::flat_hash_set<int> found_bases;
      for (int i = 0; i < path_starts_.size(); ++i) {
        int index = new_index;
        // Note: old and new path starts are sorted by construction.
        while (index < new_path_starts.size() &&
               new_path_starts[index] < path_starts_[i]) {
          ++index;
        }
        const bool found = (index < new_path_starts.size() &&
                            new_path_starts[index] == path_starts_[i]);
        if (found) {
          new_index = index;
        }
        for (int j = 0; j < iteration_parameters_.number_of_base_nodes; ++j) {
          if (base_paths_[j] == i && !found_bases.contains(j)) {
            found_bases.insert(j);
            base_paths_[j] = new_index;
            // If the current position of the base node is a removed empty path,
            // readjusting it to the last visited path start.
            if (!found) {
              base_nodes_[j] = new_path_starts[new_index];
            }
          }
        }
      }
    }
    path_starts_.swap(new_path_starts);
    // For every base path, store the end corresponding to the path start.
    // TODO(user): make this faster, maybe by pairing starts with ends.
    path_ends_.clear();
    path_ends_.reserve(path_starts_.size());
    int64_t max_node_index = number_of_nexts_ - 1;
    for (const int start_node : path_starts_) {
      int64_t node = start_node;
      while (!IsPathEnd(node)) node = OldNext(node);
      path_ends_.push_back(node);
      max_node_index = std::max(max_node_index, node);
    }
    node_path_starts_.assign(max_node_index + 1, -1);
    node_path_ends_.assign(max_node_index + 1, -1);
    for (int i = 0; i < path_starts_.size(); ++i) {
      const int64_t start_node = path_starts_[i];
      const int64_t end_node = path_ends_[i];
      int64_t node = start_node;
      while (!IsPathEnd(node)) {
        node_path_starts_[node] = start_node;
        node_path_ends_[node] = end_node;
        node = OldNext(node);
      }
      node_path_starts_[node] = start_node;
      node_path_ends_[node] = end_node;
    }
  }
  void InitializeInactives() {
    inactives_.clear();
    for (int i = 0; i < number_of_nexts_; ++i) {
      inactives_.push_back(OldNext(i) == i);
    }
  }
  void InitializeBaseNodes() {
    // Inactive nodes must be detected before determining new path starts.
    InitializeInactives();
    InitializePathStarts();
    if (first_start_ || InitPosition()) {
      // Only do this once since the following starts will continue from the
      // preceding position
      for (int i = 0; i < iteration_parameters_.number_of_base_nodes; ++i) {
        base_paths_[i] = 0;
        base_nodes_[i] = path_starts_[0];
      }
      first_start_ = false;
    }
    for (int i = 0; i < iteration_parameters_.number_of_base_nodes; ++i) {
      // If base node has been made inactive, restart from path start.
      int64_t base_node = base_nodes_[i];
      if (RestartAtPathStartOnSynchronize() || IsInactive(base_node)) {
        base_node = path_starts_[base_paths_[i]];
        base_nodes_[i] = base_node;
      }
      end_nodes_[i] = base_node;
    }
    // Repair end_nodes_ in case some must be on the same path and are not
    // anymore (due to other operators moving these nodes).
    for (int i = 1; i < iteration_parameters_.number_of_base_nodes; ++i) {
      if (OnSamePathAsPreviousBase(i) &&
          !OnSamePath(base_nodes_[i - 1], base_nodes_[i])) {
        const int64_t base_node = base_nodes_[i - 1];
        base_nodes_[i] = base_node;
        end_nodes_[i] = base_node;
        base_paths_[i] = base_paths_[i - 1];
      }
    }
    for (int i = 0; i < iteration_parameters_.number_of_base_nodes; ++i) {
      base_node_iterators_[i].Reset(/*update_end_nodes=*/true);
    }
    just_started_ = true;
  }
  void InitializeAlternatives() {
    active_in_alternative_set_.resize(alternative_sets_.size(), -1);
    for (int i = 0; i < alternative_sets_.size(); ++i) {
      const int64_t current_active = active_in_alternative_set_[i];
      if (current_active >= 0 && !IsInactive(current_active)) continue;
      for (int64_t index : alternative_sets_[i]) {
        if (!IsInactive(index)) {
          active_in_alternative_set_[i] = index;
          break;
        }
      }
    }
  }

  class ActivePaths {
   public:
    explicit ActivePaths(int num_nodes) : start_to_path_(num_nodes, -1) {}
    void Clear() { to_reset_ = true; }
    template <typename T>
    void Initialize(T is_start) {
      if (is_path_pair_active_.empty()) {
        num_paths_ = 0;
        absl::c_fill(start_to_path_, -1);
        for (int i = 0; i < start_to_path_.size(); ++i) {
          if (is_start(i)) {
            start_to_path_[i] = num_paths_;
            ++num_paths_;
          }
        }
      }
    }
    void DeactivatePathPair(int start1, int start2) {
      if (to_reset_) Reset();
      is_path_pair_active_[start_to_path_[start1] * num_paths_ +
                           start_to_path_[start2]] = false;
    }
    void ActivatePath(int start) {
      if (to_reset_) Reset();
      const int p1 = start_to_path_[start];
      const int p1_block = num_paths_ * p1;
      for (int p2 = 0; p2 < num_paths_; ++p2) {
        is_path_pair_active_[p1_block + p2] = true;
      }
      for (int p2_block = 0; p2_block < is_path_pair_active_.size();
           p2_block += num_paths_) {
        is_path_pair_active_[p2_block + p1] = true;
      }
    }
    bool IsPathPairActive(int start1, int start2) const {
      if (to_reset_) return true;
      return is_path_pair_active_[start_to_path_[start1] * num_paths_ +
                                  start_to_path_[start2]];
    }

   private:
    void Reset() {
      if (!to_reset_) return;
      is_path_pair_active_.assign(num_paths_ * num_paths_, true);
      to_reset_ = false;
    }

    bool to_reset_ = true;
    int num_paths_ = 0;
    std::vector<int64_t> start_to_path_;
    std::vector<bool> is_path_pair_active_;
  };

  std::unique_ptr<int[]> base_nodes_;
  std::unique_ptr<int[]> end_nodes_;
  std::unique_ptr<int[]> base_paths_;
  std::vector<int> node_path_starts_;
  std::vector<int> node_path_ends_;
  bool iterators_initialized_ = false;
#ifndef SWIG
  std::vector<BaseNodeIterators<PathOperator>> base_node_iterators_;
#endif  // SWIG
  std::vector<int64_t> path_starts_;
  std::vector<int64_t> path_ends_;
  std::vector<bool> inactives_;
  bool just_started_;
  bool first_start_;
  int next_base_to_increment_;
  IterationParameters iteration_parameters_;
  bool optimal_paths_enabled_;
  std::vector<int> path_basis_;
  ActivePaths active_paths_;
  /// Node alternative data.
#ifndef SWIG
  std::vector<std::vector<int64_t>> alternative_sets_;
#endif  // SWIG
  std::vector<int> alternative_index_;
  std::vector<int64_t> active_in_alternative_set_;
  std::vector<int> sibling_alternative_;

  friend class BaseNodeIterators<PathOperator>;
  friend class AlternativeNodeIterator;
  friend class NodeNeighborIterator;
};

#ifndef SWIG
/// ----- 2Opt -----

/// Reverses a sub-chain of a path. It is called 2Opt because it breaks
/// 2 arcs on the path; resulting paths are called 2-optimal.
/// Possible neighbors for the path 1 -> 2 -> 3 -> 4 -> 5
/// (where (1, 5) are first and last nodes of the path and can therefore not be
/// moved):
/// 1 -> 3 -> 2 -> 4 -> 5
/// 1 -> 4 -> 3 -> 2 -> 5
/// 1 -> 2 -> 4 -> 3 -> 5

LocalSearchOperator* MakeTwoOpt(
    Solver* solver, const std::vector<IntVar*>& vars,
    const std::vector<IntVar*>& secondary_vars,
    std::function<int(int64_t)> start_empty_path_class,
    std::function<const std::vector<int>&(int, int)> get_incoming_neighbors =
        nullptr,
    std::function<const std::vector<int>&(int, int)> get_outgoing_neighbors =
        nullptr);

/// ----- Relocate -----

/// Moves a sub-chain of a path to another position; the specified chain length
/// is the fixed length of the chains being moved. When this length is 1 the
/// operator simply moves a node to another position.
/// Possible neighbors for the path 1 -> 2 -> 3 -> 4 -> 5, for a chain length
/// of 2 (where (1, 5) are first and last nodes of the path and can
/// therefore not be moved):
/// 1 -> 4 -> 2 -> 3 -> 5
/// 1 -> 3 -> 4 -> 2 -> 5

LocalSearchOperator* MakeRelocate(
    Solver* solver, const std::vector<IntVar*>& vars,
    const std::vector<IntVar*>& secondary_vars,
    std::function<int(int64_t)> start_empty_path_class,
    std::function<const std::vector<int>&(int, int)> get_incoming_neighbors =
        nullptr,
    std::function<const std::vector<int>&(int, int)> get_outgoing_neighbors =
        nullptr);

/// ----- OrOpt -----

/// Variant of Relocate  which relocates chains of nodes of length 1, 2 and 3
/// within the same path. The OrOpt operator is a limited version of 3Opt
/// (breaking 3 arcs on a path).
LocalSearchOperator* MakeOrOpt(
    Solver* solver, const std::vector<IntVar*>& vars,
    const std::vector<IntVar*>& secondary_vars,
    std::function<int(int64_t)> start_empty_path_class);

/// ----- Exchange -----

/// Exchanges the positions of two nodes.
/// Possible neighbors for the path 1 -> 2 -> 3 -> 4 -> 5
/// (where (1, 5) are first and last nodes of the path and can therefore not
/// be moved):
/// 1 -> 3 -> 2 -> 4 -> 5
/// 1 -> 4 -> 3 -> 2 -> 5
/// 1 -> 2 -> 4 -> 3 -> 5

LocalSearchOperator* MakeExchange(
    Solver* solver, const std::vector<IntVar*>& vars,
    const std::vector<IntVar*>& secondary_vars,
    std::function<int(int64_t)> start_empty_path_class,
    std::function<const std::vector<int>&(int, int)> get_incoming_neighbors =
        nullptr,
    std::function<const std::vector<int>&(int, int)> get_outgoing_neighbors =
        nullptr);

/// ----- Cross -----

/// Cross echanges the starting chains of 2 paths, including exchanging the
/// whole paths.
/// First and last nodes are not moved.
/// Possible neighbors for the paths 1 -> 2 -> 3 -> 4 -> 5 and 6 -> 7 -> 8
/// (where (1, 5) and (6, 8) are first and last nodes of the paths and can
/// therefore not be moved):
/// 1 -> 7 -> 3 -> 4 -> 5  6 -> 2 -> 8
/// 1 -> 7 -> 4 -> 5       6 -> 2 -> 3 -> 8
/// 1 -> 7 -> 5            6 -> 2 -> 3 -> 4 -> 8

LocalSearchOperator* MakeCross(
    Solver* solver, const std::vector<IntVar*>& vars,
    const std::vector<IntVar*>& secondary_vars,
    std::function<int(int64_t)> start_empty_path_class,
    std::function<const std::vector<int>&(int, int)> get_incoming_neighbors =
        nullptr,
    std::function<const std::vector<int>&(int, int)> get_outgoing_neighbors =
        nullptr);

/// ----- MakeActive -----

/// MakeActive inserts an inactive node into a path.
/// Possible neighbors for the path 1 -> 2 -> 3 -> 4 with 5 inactive (where 1
/// and 4 are first and last nodes of the path) are:
/// 1 -> 5 -> 2 -> 3 -> 4
/// 1 -> 2 -> 5 -> 3 -> 4
/// 1 -> 2 -> 3 -> 5 -> 4

LocalSearchOperator* MakeActive(
    Solver* solver, const std::vector<IntVar*>& vars,
    const std::vector<IntVar*>& secondary_vars,
    std::function<int(int64_t)> start_empty_path_class,
    std::function<const std::vector<int>&(int, int)> get_incoming_neighbors =
        nullptr,
    std::function<const std::vector<int>&(int, int)> get_outgoing_neighbors =
        nullptr);

/// ---- RelocateAndMakeActive -----

/// RelocateAndMakeActive relocates a node and replaces it by an inactive node.
/// The idea is to make room for inactive nodes.
/// Possible neighbor for paths 0 -> 4, 1 -> 2 -> 5 and 3 inactive is:
/// 0 -> 2 -> 4, 1 -> 3 -> 5.
/// TODO(user): Naming is close to MakeActiveAndRelocate but this one is
/// correct; rename MakeActiveAndRelocate if it is actually used.

LocalSearchOperator* RelocateAndMakeActive(
    Solver* solver, const std::vector<IntVar*>& vars,
    const std::vector<IntVar*>& secondary_vars,
    std::function<int(int64_t)> start_empty_path_class);

// ----- ExchangeAndMakeActive -----

// ExchangeAndMakeActive exchanges two nodes and inserts an inactive node.
// Possible neighbors for paths 0 -> 2 -> 4, 1 -> 3 -> 6 and 5 inactive are:
// 0 -> 3 -> 4, 1 -> 5 -> 2 -> 6
// 0 -> 3 -> 5 -> 4, 1 -> 2 -> 6
// 0 -> 5 -> 3 -> 4, 1 -> 2 -> 6
// 0 -> 3 -> 4, 1 -> 2 -> 5 -> 6
//
// Warning this operator creates a very large neighborhood, with O(m*n^3)
// neighbors (n: number of active nodes, m: number of non active nodes).
// It should be used with only a small number of non active nodes.
// TODO(user): Add support for neighbors which would make this operator more
// usable.

LocalSearchOperator* ExchangeAndMakeActive(
    Solver* solver, const std::vector<IntVar*>& vars,
    const std::vector<IntVar*>& secondary_vars,
    std::function<int(int64_t)> start_empty_path_class);

// ----- ExchangePathEndsAndMakeActive -----

// An operator which exchanges the first and last nodes of two paths and makes a
// node active.
// Possible neighbors for paths 0 -> 1 -> 2 -> 7, 6 -> 3 -> 4 -> 8
// and 5 inactive are:
// 0 -> 5 -> 3 -> 4 -> 7, 6 -> 1 -> 2 -> 8
// 0 -> 3 -> 4 -> 7, 6 -> 1 -> 5 -> 2 -> 8
// 0 -> 3 -> 4 -> 7, 6 -> 1 -> 2 -> 5 -> 8
// 0 -> 3 -> 5 -> 4 -> 7, 6 -> 1 -> 2 -> 8
// 0 -> 3 -> 4 -> 5 -> 7, 6 -> 1 -> 2 -> 8
//
// This neighborhood is an artificially reduced version of
// ExchangeAndMakeActiveOperator. It can still be used to opportunistically
// insert inactive nodes.

LocalSearchOperator* ExchangePathStartEndsAndMakeActive(
    Solver* solver, const std::vector<IntVar*>& vars,
    const std::vector<IntVar*>& secondary_vars,
    std::function<int(int64_t)> start_empty_path_class);

/// ----- MakeActiveAndRelocate -----

/// MakeActiveAndRelocate makes a node active next to a node being relocated.
/// Possible neighbor for paths 0 -> 4, 1 -> 2 -> 5 and 3 inactive is:
/// 0 -> 3 -> 2 -> 4, 1 -> 5.

LocalSearchOperator* MakeActiveAndRelocate(
    Solver* solver, const std::vector<IntVar*>& vars,
    const std::vector<IntVar*>& secondary_vars,
    std::function<int(int64_t)> start_empty_path_class);

/// ----- MakeInactive -----

/// MakeInactive makes path nodes inactive.
/// Possible neighbors for the path 1 -> 2 -> 3 -> 4 (where 1 and 4 are first
/// and last nodes of the path) are:
/// 1 -> 3 -> 4 & 2 inactive
/// 1 -> 2 -> 4 & 3 inactive

LocalSearchOperator* MakeInactive(
    Solver* solver, const std::vector<IntVar*>& vars,
    const std::vector<IntVar*>& secondary_vars,
    std::function<int(int64_t)> start_empty_path_class);

/// ----- RelocateAndMakeInactive -----

/// RelocateAndMakeInactive relocates a node to a new position and makes the
/// node which was at that position inactive.
/// Possible neighbors for paths 0 -> 2 -> 4, 1 -> 3 -> 5 are:
/// 0 -> 3 -> 4, 1 -> 5 & 2 inactive
/// 0 -> 4, 1 -> 2 -> 5 & 3 inactive

LocalSearchOperator* RelocateAndMakeInactive(
    Solver* solver, const std::vector<IntVar*>& vars,
    const std::vector<IntVar*>& secondary_vars,
    std::function<int(int64_t)> start_empty_path_class);

/// ----- MakeChainInactive -----

/// Operator which makes a "chain" of path nodes inactive.
/// Possible neighbors for the path 1 -> 2 -> 3 -> 4 (where 1 and 4 are first
/// and last nodes of the path) are:
///   1 -> 3 -> 4 with 2 inactive
///   1 -> 2 -> 4 with 3 inactive
///   1 -> 4 with 2 and 3 inactive

LocalSearchOperator* MakeChainInactive(
    Solver* solver, const std::vector<IntVar*>& vars,
    const std::vector<IntVar*>& secondary_vars,
    std::function<int(int64_t)> start_empty_path_class);

/// ----- SwapActive -----

/// SwapActive replaces an active node by an inactive one.
/// Possible neighbors for the path 1 -> 2 -> 3 -> 4 with 5 inactive (where 1
/// and 4 are first and last nodes of the path) are:
/// 1 -> 5 -> 3 -> 4 & 2 inactive
/// 1 -> 2 -> 5 -> 4 & 3 inactive

LocalSearchOperator* MakeSwapActive(
    Solver* solver, const std::vector<IntVar*>& vars,
    const std::vector<IntVar*>& secondary_vars,
    std::function<int(int64_t)> start_empty_path_class);

/// ----- SwapActiveChain -----

LocalSearchOperator* MakeSwapActiveChain(
    Solver* solver, const std::vector<IntVar*>& vars,
    const std::vector<IntVar*>& secondary_vars,
    std::function<int(int64_t)> start_empty_path_class, int max_chain_size);

/// ----- ExtendedSwapActive -----

/// ExtendedSwapActive makes an inactive node active and an active one
/// inactive. It is similar to SwapActiveOperator excepts that it tries to
/// insert the inactive node in all possible positions instead of just the
/// position of the node made inactive.
/// Possible neighbors for the path 1 -> 2 -> 3 -> 4 with 5 inactive (where 1
/// and 4 are first and last nodes of the path) are:
/// 1 -> 5 -> 3 -> 4 & 2 inactive
/// 1 -> 3 -> 5 -> 4 & 2 inactive
/// 1 -> 5 -> 2 -> 4 & 3 inactive
/// 1 -> 2 -> 5 -> 4 & 3 inactive

LocalSearchOperator* MakeExtendedSwapActive(
    Solver* solver, const std::vector<IntVar*>& vars,
    const std::vector<IntVar*>& secondary_vars,
    std::function<int(int64_t)> start_empty_path_class);

/// ----- TSP-based operators -----

/// Sliding TSP operator
/// Uses an exact dynamic programming algorithm to solve the TSP corresponding
/// to path sub-chains.
/// For a subchain 1 -> 2 -> 3 -> 4 -> 5 -> 6, solves the TSP on nodes A, 2, 3,
/// 4, 5, where A is a merger of nodes 1 and 6 such that cost(A,i) = cost(1,i)
/// and cost(i,A) = cost(i,6).

LocalSearchOperator* MakeTSPOpt(Solver* solver,
                                const std::vector<IntVar*>& vars,
                                const std::vector<IntVar*>& secondary_vars,
                                Solver::IndexEvaluator3 evaluator,
                                int chain_length);

/// TSP-base lns.
/// Randomly merge consecutive nodes until n "meta"-nodes remain and solve the
/// corresponding TSP. This can be seen as a large neighborhood search operator
/// although decisions are taken with the operator.
/// This is an "unlimited" neighborhood which must be stopped by search limits.
/// To force diversification, the operator iteratively forces each node to serve
/// as base of a meta-node.

LocalSearchOperator* MakeTSPLns(Solver* solver,
                                const std::vector<IntVar*>& vars,
                                const std::vector<IntVar*>& secondary_vars,
                                Solver::IndexEvaluator3 evaluator,
                                int tsp_size);

/// ----- Lin-Kernighan -----

LocalSearchOperator* MakeLinKernighan(
    Solver* solver, const std::vector<IntVar*>& vars,
    const std::vector<IntVar*>& secondary_vars,
    const Solver::IndexEvaluator3& evaluator, bool topt);

/// ----- Path-based Large Neighborhood Search -----

/// Breaks "number_of_chunks" chains of "chunk_size" arcs, and deactivate all
/// inactive nodes if "unactive_fragments" is true.
/// As a special case, if chunk_size=0, then we break full paths.

LocalSearchOperator* MakePathLns(Solver* solver,
                                 const std::vector<IntVar*>& vars,
                                 const std::vector<IntVar*>& secondary_vars,
                                 int number_of_chunks, int chunk_size,
                                 bool unactive_fragments);

/// After building a Directed Acyclic Graph, allows to generate sub-DAGs
/// reachable from a node.
/// Workflow:
/// - Call AddArc() repeatedly to add arcs describing a DAG. Nodes are allocated
///   on the user side, they must be nonnegative, and it is better but not
///   mandatory for the set of nodes to be dense.
/// - Call BuildGraph(). This precomputes all the information needed to make
///   subsequent requests for sub-DAGs.
/// - Call ComputeSortedSubDagArcs(node). This returns a view to arcs reachable
///   from node, in topological order.
/// All arcs must be added before calling BuildGraph(),
/// and ComputeSortedSubDagArcs() can only be called after BuildGraph().
/// If the arcs form a graph that has directed cycles,
/// - in debug mode, BuildGraph() will crash.
/// - otherwise, BuildGraph() will not crash, but ComputeSortedSubDagArcs()
///   will only return a subset of arcs reachable by the given node.
class SubDagComputer {
 public:
  DEFINE_STRONG_INT_TYPE(ArcId, int);
  DEFINE_STRONG_INT_TYPE(NodeId, int);
  SubDagComputer() = default;
  // Adds an arc between node 'tail' and node 'head'. Nodes must be nonnegative.
  // Returns the index of the new arc, those are used to identify arcs when
  // calling ComputeSortedSubDagArcs().
  ArcId AddArc(NodeId tail, NodeId head) {
    DCHECK(!graph_was_built_);
    num_nodes_ = std::max(num_nodes_, std::max(tail.value(), head.value()) + 1);
    const ArcId arc_id(arcs_.size());
    arcs_.push_back({.tail = tail, .head = head, .arc_id = arc_id});
    return arc_id;
  }
  // Finishes the construction of the DAG.  'num_nodes' is the number of nodes
  // in the DAG and must be greater than all node indices passed to AddArc().
  void BuildGraph(int num_nodes);
  // Computes the arcs of the sub-DAG reachable from node returns a view of
  // those arcs in topological order.
  const std::vector<ArcId>& ComputeSortedSubDagArcs(NodeId node);

 private:
  // Checks whether the underlying graph has a directed cycle.
  // Should be called after the graph has been built.
  bool HasDirectedCycle() const;

  struct Arc {
    NodeId tail;
    NodeId head;
    ArcId arc_id;
    bool operator<(const Arc& other) const {
      return std::tie(tail, arc_id) < std::tie(other.tail, other.arc_id);
    }
  };
  int num_nodes_ = 0;
  std::vector<Arc> arcs_;
  // Initialized by BuildGraph(), after which the outgoing arcs of node n are
  // the range from arcs_[arcs_of_node_[n]] included to
  // arcs_[arcs_of_node_[n+1]] excluded.
  util_intops::StrongVector<NodeId, int> arcs_of_node_;
  // Must be false before BuildGraph() is called, true afterwards.
  bool graph_was_built_ = false;
  // Used by ComputeSortedSubDagArcs.
  util_intops::StrongVector<NodeId, int> indegree_of_node_;
  // Used by ComputeSortedSubDagArcs.
  std::vector<NodeId> nodes_to_visit_;
  // Used as output, set up as a member to allow reuse.
  std::vector<ArcId> sorted_arcs_;
};

// A LocalSearchState is a container for variables with domains that can be
// relaxed and tightened, saved and restored. It represents the solution state
// of a local search engine, and allows it to go from solution to solution by
// relaxing some variables to form a new subproblem, then tightening those
// variables to move to a new solution representation. That state may be saved
// to an internal copy, or reverted to the last saved internal copy.
// Relaxing a variable returns its bounds to their initial state.
// Tightening a variable's bounds may make its min larger than its max,
// in that case, the tightening function will return false, and the state will
// be marked as invalid. No other operations than Revert() can be called on an
// invalid state: in particular, an invalid state cannot be saved.
class LocalSearchState {
 public:
  class Variable;
  DEFINE_STRONG_INT_TYPE(VariableDomainId, int);
  DEFINE_STRONG_INT_TYPE(ConstraintId, int);
  // Adds a variable domain to this state, returns a handler to the new domain.
  VariableDomainId AddVariableDomain(int64_t relaxed_min, int64_t relaxed_max);
  // Relaxes the domain, returns false iff the domain was already relaxed.
  bool RelaxVariableDomain(VariableDomainId domain_id);
  bool TightenVariableDomainMin(VariableDomainId domain_id, int64_t value);
  bool TightenVariableDomainMax(VariableDomainId domain_id, int64_t value);
  int64_t VariableDomainMin(VariableDomainId domain_id) const;
  int64_t VariableDomainMax(VariableDomainId domain_id) const;
  void ChangeRelaxedVariableDomain(VariableDomainId domain_id, int64_t min,
                                   int64_t max);

  // Propagation of all events.
  void PropagateRelax(VariableDomainId domain_id);
  bool PropagateTighten(VariableDomainId domain_id);
  // Makes a variable, an object with restricted operations on the underlying
  // domain identified by domain_id: only Relax, Tighten and Min/Max read
  // operations are available.
  Variable MakeVariable(VariableDomainId domain_id);
  // Makes a variable from an interval without going through a domain_id.
  // Can be used when no direct manipulation of the domain is needed.
  Variable MakeVariableWithRelaxedDomain(int64_t min, int64_t max);
  // Makes a variable with no state, this is meant as a placeholder.
  static Variable DummyVariable();
  void Commit();
  void Revert();
  bool StateIsFeasible() const {
    return state_domains_are_all_nonempty_ && num_committed_empty_domains_ == 0;
  }
  // Adds a constraint that output = input_offset + sum_i weight_i * input_i.
  void AddWeightedSumConstraint(
      const std::vector<VariableDomainId>& input_domain_ids,
      const std::vector<int64_t>& input_weights, int64_t input_offset,
      VariableDomainId output_domain_id);
  // Precomputes which domain change triggers which constraint(s).
  // Should be run after adding all constraints, before any Relax()/Tighten().
  void CompileConstraints();

 private:
  // VariableDomains implement the domain of Variables.
  // Those are trailed, meaning they are saved on their first modification,
  // and can be reverted or committed in O(1) per modification.
  struct VariableDomain {
    int64_t min;
    int64_t max;
  };
  bool IntersectionIsEmpty(const VariableDomain& d1,
                           const VariableDomain& d2) const {
    return d1.max < d2.min || d2.max < d1.min;
  }
  util_intops::StrongVector<VariableDomainId, VariableDomain> relaxed_domains_;
  util_intops::StrongVector<VariableDomainId, VariableDomain> current_domains_;
  struct TrailedVariableDomain {
    VariableDomain committed_domain;
    VariableDomainId domain_id;
  };
  std::vector<TrailedVariableDomain> trailed_domains_;
  util_intops::StrongVector<VariableDomainId, bool> domain_is_trailed_;
  // True iff all domains have their min <= max.
  bool state_domains_are_all_nonempty_ = true;
  bool state_has_relaxed_domains_ = false;
  // Number of domains d for which the intersection of
  // current_domains_[d] and relaxed_domains_[d] is empty.
  int num_committed_empty_domains_ = 0;
  int trailed_num_committed_empty_domains_ = 0;

  // Constraints may be trailed too, they decide how to track their internal
  // structure.
  class Constraint;
  void TrailConstraint(Constraint* constraint) {
    trailed_constraints_.push_back(constraint);
  }
  std::vector<Constraint*> trailed_constraints_;

  // Stores domain-constraint dependencies, allows to generate topological
  // orderings of dependency arcs reachable from nodes.
  class DependencyGraph {
   public:
    DependencyGraph() {}
    // There are two kinds of domain-constraint dependencies:
    // - domain -> constraint when the domain is an input to the constraint.
    //   Then the label is the index of the domain in the input tuple.
    // - constraint -> domain when the domain is the output of the constraint.
    //   Then, the label is -1.
    struct Dependency {
      VariableDomainId domain_id;
      int input_index;
      ConstraintId constraint_id;
    };
    // Adds all dependencies domains[i] -> constraint labelled by i.
    void AddDomainsConstraintDependencies(
        const std::vector<VariableDomainId>& domain_ids,
        ConstraintId constraint_id);
    // Adds a dependency domain -> constraint labelled by -1.
    void AddConstraintDomainDependency(ConstraintId constraint_id,
                                       VariableDomainId domain_id);
    // After all dependencies have been added,
    // builds the DAG representation that allows to compute sorted dependencies.
    void BuildDependencyDAG(int num_domains);
    // Returns a view on the list of arc dependencies reachable by given domain,
    // in some topological order of the overall DAG. Modifying the graph or
    // calling ComputeSortedDependencies() again invalidates the view.
    const std::vector<Dependency>& ComputeSortedDependencies(
        VariableDomainId domain_id);

   private:
    using ArcId = SubDagComputer::ArcId;
    using NodeId = SubDagComputer::NodeId;
    // Returns dag_node_of_domain_[domain_id] if already defined,
    // or makes a node for domain_id, possibly extending dag_node_of_domain_.
    NodeId GetOrCreateNodeOfDomainId(VariableDomainId domain_id);
    // Returns dag_node_of_constraint_[constraint_id] if already defined,
    // or makes a node for constraint_id, possibly extending
    // dag_node_of_constraint_.
    NodeId GetOrCreateNodeOfConstraintId(ConstraintId constraint_id);
    // Structure of the expression DAG, used to buffer propagation storage.
    SubDagComputer dag_;
    // Maps arcs of dag_ to domain/constraint dependencies.
    util_intops::StrongVector<ArcId, Dependency> dependency_of_dag_arc_;
    // Maps domain ids to dag_ nodes.
    util_intops::StrongVector<VariableDomainId, NodeId> dag_node_of_domain_;
    // Maps constraint ids to dag_ nodes.
    util_intops::StrongVector<ConstraintId, NodeId> dag_node_of_constraint_;
    // Number of nodes currently allocated in dag_.
    // Reserve node 0 as a default dummy node with no dependencies.
    int num_dag_nodes_ = 1;
    // Used as reusable output of ComputeSortedDependencies().
    std::vector<Dependency> sorted_dependencies_;
  };
  DependencyGraph dependency_graph_;

  // Propagation order storage: each domain change triggers constraints.
  // Each trigger tells a constraint that a domain changed, and identifies
  // the domain by the index in the list of the constraint's inputs.
  struct Trigger {
    Constraint* constraint;
    int input_index;
  };
  // Triggers of all constraints, concatenated.
  // The triggers of domain i are stored from triggers_of_domain_[i]
  // to triggers_of_domain_[i+1] excluded.
  std::vector<Trigger> triggers_;
  util_intops::StrongVector<VariableDomainId, int> triggers_of_domain_;

  // Constraints are used to form expressions that make up the objective.
  // Constraints are directed: they have inputs and an output, moreover the
  // constraint-domain graph must not have directed cycles.
  class Constraint {
   public:
    virtual ~Constraint() = default;
    virtual LocalSearchState::VariableDomain Propagate(int input_index) = 0;
    virtual VariableDomainId Output() const = 0;
    virtual void Commit() = 0;
    virtual void Revert() = 0;
  };
  // WeightedSum constraints enforces the equation:
  //   output = offset + sum_i input_weights[i] * input_domain_ids[i]
  class WeightedSum final : public Constraint {
   public:
    WeightedSum(LocalSearchState* state,
                const std::vector<VariableDomainId>& input_domain_ids,
                const std::vector<int64_t>& input_weights, int64_t input_offset,
                VariableDomainId output);
    ~WeightedSum() override = default;
    LocalSearchState::VariableDomain Propagate(int input_index) override;
    void Commit() override;
    void Revert() override;
    VariableDomainId Output() const override { return output_; }

   private:
    // Weighted variable holds a variable's domain, an associated weight,
    // and the variable's last known min and max.
    struct WeightedVariable {
      int64_t min;
      int64_t max;
      int64_t committed_min;
      int64_t committed_max;
      int64_t weight;
      VariableDomainId domain;
      bool is_trailed;
      void Commit() {
        committed_min = min;
        committed_max = max;
        is_trailed = false;
      }
      void Revert() {
        min = committed_min;
        max = committed_max;
        is_trailed = false;
      }
    };
    std::vector<WeightedVariable> inputs_;
    std::vector<WeightedVariable*> trailed_inputs_;
    // Invariants held by this constraint to allow O(1) propagation.
    struct Invariants {
      // Number of inputs_[i].min equal to kint64min.
      int64_t num_neg_inf;
      // Sum of inputs_[i].min that are different from kint64min.
      int64_t wsum_mins;
      // Number of inputs_[i].max equal to kint64max.
      int64_t num_pos_inf;
      // Sum of inputs_[i].max that are different from kint64max.
      int64_t wsum_maxs;
    };
    Invariants invariants_;
    Invariants committed_invariants_;

    const VariableDomainId output_;
    LocalSearchState* const state_;
    bool constraint_is_trailed_ = false;
  };
  // Used to identify constraints and hold ownership.
  util_intops::StrongVector<ConstraintId, std::unique_ptr<Constraint>>
      constraints_;
};

// A LocalSearchState Variable can only be created by a LocalSearchState,
// then it is meant to be passed by copy. If at some point the duplication of
// LocalSearchState pointers is too expensive, we could switch to index only,
// and the user would have to know the relevant state. The present setup allows
// to ensure that variable users will not misuse the state.
class LocalSearchState::Variable {
 public:
  Variable() : state_(nullptr), domain_id_(VariableDomainId(-1)) {}
  int64_t Min() const {
    DCHECK(Exists());
    return state_->VariableDomainMin(domain_id_);
  }
  int64_t Max() const {
    DCHECK(Exists());
    return state_->VariableDomainMax(domain_id_);
  }
  // Sets variable's minimum to max(Min(), new_min) and propagates the change.
  // Returns true iff the variable domain is nonempty and propagation succeeded.
  bool SetMin(int64_t new_min) const {
    if (!Exists()) return true;
    return state_->TightenVariableDomainMin(domain_id_, new_min) &&
           state_->PropagateTighten(domain_id_);
  }
  // Sets variable's maximum to min(Max(), new_max) and propagates the change.
  // Returns true iff the variable domain is nonempty and propagation succeeded.
  bool SetMax(int64_t new_max) const {
    if (!Exists()) return true;
    return state_->TightenVariableDomainMax(domain_id_, new_max) &&
           state_->PropagateTighten(domain_id_);
  }
  void Relax() const {
    if (state_ == nullptr) return;
    if (state_->RelaxVariableDomain(domain_id_)) {
      state_->PropagateRelax(domain_id_);
    }
  }
  bool Exists() const { return state_ != nullptr; }

 private:
  // Only LocalSearchState can construct LocalSearchVariables.
  friend class LocalSearchState;

  Variable(LocalSearchState* state, VariableDomainId domain_id)
      : state_(state), domain_id_(domain_id) {}

  LocalSearchState* state_;
  VariableDomainId domain_id_;
};
#endif  // !defined(SWIG)

/// Local Search Filters are used for fast neighbor pruning.
/// Filtering a move is done in several phases:
/// - in the Relax phase, filters determine which parts of their internals
///   will be changed by the candidate, and modify intermediary State
/// - in the Accept phase, filters check that the candidate is feasible,
/// - if the Accept phase succeeds, the solver may decide to trigger a
///   Synchronize phase that makes filters change their internal representation
///   to the last candidate,
/// - otherwise (Accept fails or the solver does not want to synchronize),
///   a Revert phase makes filters erase any intermediary State generated by the
///   Relax and Accept phases.
/// A given filter has phases called with the following pattern:
/// (Relax.Accept.Synchronize | Relax.Accept.Revert | Relax.Revert)*.
/// Filters's Revert() is always called in the reverse order their Accept() was
/// called, to allow late filters to use state done/undone by early filters'
/// Accept()/Revert().
class LocalSearchFilter : public BaseObject {
 public:
  /// Lets the filter know what delta and deltadelta will be passed in the next
  /// Accept().
  virtual void Relax(const Assignment*, const Assignment*) {}
  /// Dual of Relax(), lets the filter know that the delta was accepted.
  virtual void Commit(const Assignment*, const Assignment*) {}

  /// Accepts a "delta" given the assignment with which the filter has been
  /// synchronized; the delta holds the variables which have been modified and
  /// their new value.
  /// If the filter represents a part of the global objective, its contribution
  /// must be between objective_min and objective_max.
  /// Sample: supposing one wants to maintain a[0,1] + b[0,1] <= 1,
  /// for the assignment (a,1), (b,0), the delta (b,1) will be rejected
  /// but the delta (a,0) will be accepted.
  /// TODO(user): Remove arguments when there are no more need for those.
  virtual bool Accept(const Assignment* delta, const Assignment* deltadelta,
                      int64_t objective_min, int64_t objective_max) = 0;
  virtual bool IsIncremental() const { return false; }

  /// Synchronizes the filter with the current solution, delta being the
  /// difference with the solution passed to the previous call to Synchronize()
  /// or IncrementalSynchronize(). 'delta' can be used to incrementally
  /// synchronizing the filter with the new solution by only considering the
  /// changes in delta.
  virtual void Synchronize(const Assignment* assignment,
                           const Assignment* delta) = 0;
  /// Cancels the changes made by the last Relax()/Accept() calls.
  virtual void Revert() {}

  /// Sets the filter to empty solution.
  virtual void Reset() {}

  /// Objective value from last time Synchronize() was called.
  virtual int64_t GetSynchronizedObjectiveValue() const { return 0LL; }
  /// Objective value from the last time Accept() was called and returned true.
  // If the last Accept() call returned false, returns an undefined value.
  virtual int64_t GetAcceptedObjectiveValue() const { return 0LL; }
};

/// Filter manager: when a move is made, filters are executed to decide whether
/// the solution is feasible and compute parts of the new cost. This class
/// schedules filter execution and composes costs as a sum.
class LocalSearchFilterManager : public BaseObject {
 public:
  // This class is responsible for calling filters methods in a correct order.
  // For now, an order is specified explicitly by the user.
  enum FilterEventType { kAccept, kRelax };
  struct FilterEvent {
    LocalSearchFilter* filter;
    FilterEventType event_type;
    int priority;
  };

  std::string DebugString() const override {
    return "LocalSearchFilterManager";
  }
  // Builds a manager that calls filter methods ordered by increasing priority.
  // Note that some filters might appear only once, if their Relax() or Accept()
  // are trivial.
  explicit LocalSearchFilterManager(std::vector<FilterEvent> filter_events);
  // Builds a manager that calls filter methods using the following ordering:
  // first Relax() in vector order, then Accept() in vector order.
  explicit LocalSearchFilterManager(std::vector<LocalSearchFilter*> filters);

  // Calls Revert() of filters, in reverse order of Relax events.
  void Revert();
  /// Returns true iff all filters return true, and the sum of their accepted
  /// objectives is between objective_min and objective_max.
  /// The monitor has its Begin/EndFiltering events triggered.
  bool Accept(LocalSearchMonitor* monitor, const Assignment* delta,
              const Assignment* deltadelta, int64_t objective_min,
              int64_t objective_max);
  /// Synchronizes all filters to assignment.
  void Synchronize(const Assignment* assignment, const Assignment* delta);
  int64_t GetSynchronizedObjectiveValue() const { return synchronized_value_; }
  int64_t GetAcceptedObjectiveValue() const { return accepted_value_; }

 private:
  // Finds the last event (incremental -itself- or not) with the same priority
  // as the last incremental event.
  void FindIncrementalEventEnd();

  std::vector<FilterEvent> events_;
  int last_event_called_ = -1;
  // If a filter is incremental, its Relax() and Accept() must be called for
  // every candidate, even if the Accept() of a prior filter rejected it.
  // To ensure that those incremental filters have consistent inputs, all
  // intermediate events with Relax() must also be called.
  int incremental_events_end_ = 0;
  int64_t synchronized_value_;
  int64_t accepted_value_;
};

class OR_DLL IntVarLocalSearchFilter : public LocalSearchFilter {
 public:
  explicit IntVarLocalSearchFilter(const std::vector<IntVar*>& vars);
  ~IntVarLocalSearchFilter() override;
  /// This method should not be overridden. Override OnSynchronize() instead
  /// which is called before exiting this method.
  void Synchronize(const Assignment* assignment,
                   const Assignment* delta) override;

  bool FindIndex(IntVar* const var, int64_t* index) const {
    DCHECK(index != nullptr);
    const int var_index = var->index();
    *index = (var_index < var_index_to_index_.size())
                 ? var_index_to_index_[var_index]
                 : kUnassigned;
    return *index != kUnassigned;
  }

  /// Add variables to "track" to the filter.
  void AddVars(const std::vector<IntVar*>& vars);
  int Size() const { return vars_.size(); }
  IntVar* Var(int index) const { return vars_[index]; }
  int64_t Value(int index) const {
    DCHECK(IsVarSynced(index));
    return values_[index];
  }
  bool IsVarSynced(int index) const { return var_synced_[index]; }

 protected:
  virtual void OnSynchronize(const Assignment*) {}
  void SynchronizeOnAssignment(const Assignment* assignment);

 private:
  std::vector<IntVar*> vars_;
  std::vector<int64_t> values_;
  std::vector<bool> var_synced_;
  std::vector<int> var_index_to_index_;
  static const int kUnassigned;
};

class LocalSearchMonitor : public SearchMonitor {
  // TODO(user): Add monitoring of local search filters.
 public:
  explicit LocalSearchMonitor(Solver* solver);
  ~LocalSearchMonitor() override;
  std::string DebugString() const override { return "LocalSearchMonitor"; }

  /// Local search operator events.
  virtual void BeginOperatorStart() = 0;
  virtual void EndOperatorStart() = 0;
  virtual void BeginMakeNextNeighbor(const LocalSearchOperator* op) = 0;
  virtual void EndMakeNextNeighbor(const LocalSearchOperator* op,
                                   bool neighbor_found, const Assignment* delta,
                                   const Assignment* deltadelta) = 0;
  virtual void BeginFilterNeighbor(const LocalSearchOperator* op) = 0;
  virtual void EndFilterNeighbor(const LocalSearchOperator* op,
                                 bool neighbor_found) = 0;
  virtual void BeginAcceptNeighbor(const LocalSearchOperator* op) = 0;
  virtual void EndAcceptNeighbor(const LocalSearchOperator* op,
                                 bool neighbor_found) = 0;
  virtual void BeginFiltering(const LocalSearchFilter* filter) = 0;
  virtual void EndFiltering(const LocalSearchFilter* filter, bool reject) = 0;

  virtual bool IsActive() const = 0;

  /// Install itself on the solver.
  void Install() override;
};

}  // namespace operations_research

#endif  // ORTOOLS_CONSTRAINT_SOLVER_LOCAL_SEARCH_H_
