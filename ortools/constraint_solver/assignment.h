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

#ifndef ORTOOLS_CONSTRAINT_SOLVER_ASSIGNMENT_H_
#define ORTOOLS_CONSTRAINT_SOLVER_ASSIGNMENT_H_

#include <cstddef>
#include <cstdint>
#include <ostream>
#include <string>
#include <vector>

#include "absl/log/check.h"
#include "ortools/base/safe_hash_map.h"
#include "ortools/constraint_solver/constraint_solver.h"

namespace operations_research {

class AssignmentProto;
class IntVarAssignment;
class IntervalVarAssignment;
class SequenceVarAssignment;

class AssignmentElement {
 public:
  AssignmentElement() : activated_(true) {}
  AssignmentElement(const AssignmentElement&) = default;

  void Activate() { activated_ = true; }
  void Deactivate() { activated_ = false; }
  bool Activated() const { return activated_; }

 private:
  bool activated_;
};

class IntVarElement : public AssignmentElement {
 public:
  IntVarElement();
  explicit IntVarElement(IntVar* var);
  IntVarElement(const IntVarElement& element) = default;
  void Reset(IntVar* var);
  IntVarElement* Clone();
  void Copy(const IntVarElement& element);
  IntVar* Var() const { return var_; }
  void Store() {
    min_ = var_->Min();
    max_ = var_->Max();
  }
  void Restore() {
    if (var_ != nullptr) {
      var_->SetRange(min_, max_);
    }
  }
  void LoadFromProto(const IntVarAssignment& int_var_assignment_proto);
  void WriteToProto(IntVarAssignment* int_var_assignment_proto) const;

  int64_t Min() const { return min_; }
  void SetMin(int64_t m) { min_ = m; }
  int64_t Max() const { return max_; }
  void SetMax(int64_t m) { max_ = m; }
  int64_t Value() const {
    DCHECK_EQ(min_, max_);
    // Get the value from an unbound int var assignment element.
    return min_;
  }
  bool Bound() const { return (max_ == min_); }
  void SetRange(int64_t l, int64_t u) {
    min_ = l;
    max_ = u;
  }
  void SetValue(int64_t v) {
    min_ = v;
    max_ = v;
  }
  std::string DebugString() const;

  bool operator==(const IntVarElement& element) const;
  bool operator!=(const IntVarElement& element) const {
    return !(*this == element);
  }

 private:
  IntVar* var_;
  int64_t min_;
  int64_t max_;
};

class IntervalVarElement : public AssignmentElement {
 public:
  IntervalVarElement();
  explicit IntervalVarElement(IntervalVar* var);
  void Reset(IntervalVar* var);
  IntervalVarElement* Clone();
  void Copy(const IntervalVarElement& element);
  IntervalVar* Var() const { return var_; }
  void Store();
  void Restore();
  void LoadFromProto(
      const IntervalVarAssignment& interval_var_assignment_proto);
  void WriteToProto(IntervalVarAssignment* interval_var_assignment_proto) const;

  int64_t StartMin() const { return start_min_; }
  int64_t StartMax() const { return start_max_; }
  int64_t StartValue() const {
    CHECK_EQ(start_max_, start_min_);
    return start_max_;
  }
  int64_t DurationMin() const { return duration_min_; }
  int64_t DurationMax() const { return duration_max_; }
  int64_t DurationValue() const {
    CHECK_EQ(duration_max_, duration_min_);
    return duration_max_;
  }
  int64_t EndMin() const { return end_min_; }
  int64_t EndMax() const { return end_max_; }
  int64_t EndValue() const {
    CHECK_EQ(end_max_, end_min_);
    return end_max_;
  }
  int64_t PerformedMin() const { return performed_min_; }
  int64_t PerformedMax() const { return performed_max_; }
  int64_t PerformedValue() const {
    CHECK_EQ(performed_max_, performed_min_);
    return performed_max_;
  }
  void SetStartMin(int64_t m) { start_min_ = m; }
  void SetStartMax(int64_t m) { start_max_ = m; }
  void SetStartRange(int64_t mi, int64_t ma) {
    start_min_ = mi;
    start_max_ = ma;
  }
  void SetStartValue(int64_t v) {
    start_min_ = v;
    start_max_ = v;
  }
  void SetDurationMin(int64_t m) { duration_min_ = m; }
  void SetDurationMax(int64_t m) { duration_max_ = m; }
  void SetDurationRange(int64_t mi, int64_t ma) {
    duration_min_ = mi;
    duration_max_ = ma;
  }
  void SetDurationValue(int64_t v) {
    duration_min_ = v;
    duration_max_ = v;
  }
  void SetEndMin(int64_t m) { end_min_ = m; }
  void SetEndMax(int64_t m) { end_max_ = m; }
  void SetEndRange(int64_t mi, int64_t ma) {
    end_min_ = mi;
    end_max_ = ma;
  }
  void SetEndValue(int64_t v) {
    end_min_ = v;
    end_max_ = v;
  }
  void SetPerformedMin(int64_t m) { performed_min_ = m; }
  void SetPerformedMax(int64_t m) { performed_max_ = m; }
  void SetPerformedRange(int64_t mi, int64_t ma) {
    performed_min_ = mi;
    performed_max_ = ma;
  }
  void SetPerformedValue(int64_t v) {
    performed_min_ = v;
    performed_max_ = v;
  }
  bool Bound() const {
    return (start_min_ == start_max_ && duration_min_ == duration_max_ &&
            end_min_ == end_max_ && performed_min_ == performed_max_);
  }
  std::string DebugString() const;
  bool operator==(const IntervalVarElement& element) const;
  bool operator!=(const IntervalVarElement& element) const {
    return !(*this == element);
  }

 private:
  int64_t start_min_;
  int64_t start_max_;
  int64_t duration_min_;
  int64_t duration_max_;
  int64_t end_min_;
  int64_t end_max_;
  int64_t performed_min_;
  int64_t performed_max_;
  IntervalVar* var_;
};

class SequenceVarElement : public AssignmentElement {
 public:
  SequenceVarElement();
  explicit SequenceVarElement(SequenceVar* var);
  void Reset(SequenceVar* var);
  SequenceVarElement* Clone();
  void Copy(const SequenceVarElement& element);
  SequenceVar* Var() const { return var_; }
  void Store();
  void Restore();
  void LoadFromProto(
      const SequenceVarAssignment& sequence_var_assignment_proto);
  void WriteToProto(SequenceVarAssignment* sequence_var_assignment_proto) const;

  const std::vector<int>& ForwardSequence() const;
  const std::vector<int>& BackwardSequence() const;
  const std::vector<int>& Unperformed() const;
  void SetSequence(const std::vector<int>& forward_sequence,
                   const std::vector<int>& backward_sequence,
                   const std::vector<int>& unperformed);
  void SetForwardSequence(const std::vector<int>& forward_sequence);
  void SetBackwardSequence(const std::vector<int>& backward_sequence);
  void SetUnperformed(const std::vector<int>& unperformed);
  bool Bound() const {
    return forward_sequence_.size() + unperformed_.size() == var_->size();
  }

  std::string DebugString() const;

  bool operator==(const SequenceVarElement& element) const;
  bool operator!=(const SequenceVarElement& element) const {
    return !(*this == element);
  }

 private:
  bool CheckClassInvariants();

  SequenceVar* var_;
  std::vector<int> forward_sequence_;
  std::vector<int> backward_sequence_;
  std::vector<int> unperformed_;
};

template <class V, class E>
class AssignmentContainer {
 public:
  AssignmentContainer() = default;
  E* Add(V* var) {
    CHECK(var != nullptr);
    int index = -1;
    if (!Find(var, &index)) {
      return FastAdd(var);
    } else {
      return &elements_[index];
    }
  }
  /// Adds element without checking its presence in the container.
  E* FastAdd(V* var) {
    DCHECK(var != nullptr);
    elements_.emplace_back(var);
    return &elements_.back();
  }
  /// Advanced usage: Adds element at a given position; position has to have
  /// been allocated with AssignmentContainer::Resize() beforehand.
  E* AddAtPosition(V* var, int position) {
    elements_[position].Reset(var);
    return &elements_[position];
  }
  void Clear() {
    elements_.clear();
    if (!index_map_.empty()) {  /// 2x speedup on OR-Tools.
      index_map_.clear();
    }
  }
  /// Advanced usage: Resizes the container, potentially adding elements with
  /// null variables.
  void Resize(size_t size) { elements_.resize(size); }
  bool Empty() const { return elements_.empty(); }
  /// Copies the elements of 'container' which are already in the calling
  /// container.
  void CopyIntersection(const AssignmentContainer<V, E>& container) {
    for (int i = 0; i < container.elements_.size(); ++i) {
      const E& element = container.elements_[i];
      const V* var = element.Var();
      int index = -1;
      if (i < elements_.size() && elements_[i].Var() == var) {
        index = i;
      } else if (!Find(var, &index)) {
        continue;
      }
      DCHECK_GE(index, 0);
      E* local_element = &elements_[index];
      local_element->Copy(element);
      if (element.Activated()) {
        local_element->Activate();
      } else {
        local_element->Deactivate();
      }
    }
  }
  /// Copies all the elements of 'container' to this container, clearing its
  /// previous content.
  void Copy(const AssignmentContainer<V, E>& container) {
    Clear();
    for (const E& element : container.elements_) {
      elements_.emplace_back(element);
    }
  }
  bool Contains(const V* var) const {
    int index;
    return Find(var, &index);
  }
  E* MutableElement(const V* var) {
    E* element = MutableElementOrNull(var);
    DCHECK(element != nullptr)
        << "Unknown variable " << var->DebugString() << " in solution";
    return element;
  }
  E* MutableElementOrNull(const V* var) {
    int index = -1;
    if (Find(var, &index)) {
      return MutableElement(index);
    }
    return nullptr;
  }
  const E& Element(const V* var) const {
    const E* element = ElementPtrOrNull(var);
    DCHECK(element != nullptr)
        << "Unknown variable " << var->DebugString() << " in solution";
    return *element;
  }
  const E* ElementPtrOrNull(const V* var) const {
    int index = -1;
    if (Find(var, &index)) {
      return &Element(index);
    }
    return nullptr;
  }
  const std::vector<E>& elements() const { return elements_; }
  E* MutableElement(int index) { return &elements_[index]; }
  const E& Element(int index) const { return elements_[index]; }
  int Size() const { return elements_.size(); }
  void Store() {
    for (E& element : elements_) {
      element.Store();
    }
  }
  void Restore() {
    for (E& element : elements_) {
      if (element.Activated()) {
        element.Restore();
      }
    }
  }
  bool AreAllElementsBound() const {
    for (const E& element : elements_) {
      if (!element.Bound()) return false;
    }
    return true;
  }

  /// Returns true if this and 'container' both represent the same V* -> E map.
  /// Runs in linear time; requires that the == operator on the type E is well
  /// defined.
  bool operator==(const AssignmentContainer<V, E>& container) const {
    /// We may not have any work to do
    if (Size() != container.Size()) {
      return false;
    }
    /// The == should be order-independent
    EnsureMapIsUpToDate();
    /// Do not use the hash_map::== operator! It
    /// compares both content and how the map is hashed (e.g., number of
    /// buckets). This is not what we want.
    for (const E& element : container.elements_) {
      const auto it = index_map_.find(element.Var());
      const int position = it == index_map_.end() ? -1 : it->second;
      if (position < 0 || elements_[position] != element) {
        return false;
      }
    }
    return true;
  }
  bool operator!=(const AssignmentContainer<V, E>& container) const {
    return !(*this == container);
  }

 private:
  void EnsureMapIsUpToDate() const {
    for (int i = index_map_.size(); i < elements_.size(); ++i) {
      index_map_[elements_[i].Var()] = i;
    }
  }

  bool Find(const V* var, int* index) const {
    /// This threshold was determined from microbenchmarks on Nehalem platform.
    const size_t kMaxSizeForLinearAccess = 11;
    if (Size() <= kMaxSizeForLinearAccess) {
      /// Look for 'var' in the container by performing a linear
      /// search, avoiding the access to (and creation of) the elements
      /// hash table.
      for (int i = 0; i < elements_.size(); ++i) {
        if (var == elements_[i].Var()) {
          *index = i;
          return true;
        }
      }
      return false;
    } else {
      EnsureMapIsUpToDate();
      DCHECK_EQ(index_map_.size(), elements_.size());
      const auto it = index_map_.find(var);
      if (it == index_map_.end()) return false;
      *index = it->second;
      return true;
    }
  }

  mutable safe_hash_map<const V*, int> index_map_;
  std::vector<E> elements_;
};

/// An Assignment is a variable -> domains mapping, used
/// to report solutions to the user.
class Assignment : public PropagationBaseObject {
 public:
  typedef AssignmentContainer<IntVar, IntVarElement> IntContainer;
  typedef AssignmentContainer<IntervalVar, IntervalVarElement>
      IntervalContainer;
  typedef AssignmentContainer<SequenceVar, SequenceVarElement>
      SequenceContainer;

  explicit Assignment(Solver* solver);
  explicit Assignment(const Assignment* copy);

#ifndef SWIG
  // This type is neither copyable nor movable.
  Assignment(const Assignment&) = delete;
  Assignment& operator=(const Assignment&) = delete;
#endif

  ~Assignment() override;

  void Clear();
  bool Empty() const;
  int Size() const;
  int NumIntVars() const;
  int NumIntervalVars() const;
  int NumSequenceVars() const;
  void Store();
  void Restore();

  /// Loads an assignment from a file; does not add variables to the
  /// assignment (only the variables contained in the assignment are modified).
  bool Load(const std::string& filename);
#if !defined(SWIG)
  bool Load(File* file);
#endif  /// #if !defined(SWIG)
  void Load(const AssignmentProto& assignment_proto);
  /// Saves the assignment to a file.
  bool Save(const std::string& filename) const;
#if !defined(SWIG)
  bool Save(File* file) const;
#endif  // #if !defined(SWIG)
  void Save(AssignmentProto* assignment_proto) const;

  void AddObjective(IntVar* v);
  void AddObjectives(const std::vector<IntVar*>& vars);
  void ClearObjective();
  int NumObjectives() const;
  IntVar* Objective() const;
  IntVar* ObjectiveFromIndex(int index) const;
  bool HasObjective() const;
  bool HasObjectiveFromIndex(int index) const;
  int64_t ObjectiveMin() const;
  int64_t ObjectiveMax() const;
  int64_t ObjectiveValue() const;
  bool ObjectiveBound() const;
  void SetObjectiveMin(int64_t m);
  void SetObjectiveMax(int64_t m);
  void SetObjectiveValue(int64_t value);
  void SetObjectiveRange(int64_t l, int64_t u);
  int64_t ObjectiveMinFromIndex(int index) const;
  int64_t ObjectiveMaxFromIndex(int index) const;
  int64_t ObjectiveValueFromIndex(int index) const;
  bool ObjectiveBoundFromIndex(int index) const;
  void SetObjectiveMinFromIndex(int index, int64_t m);
  void SetObjectiveMaxFromIndex(int index, int64_t m);
  void SetObjectiveValueFromIndex(int index, int64_t value);
  void SetObjectiveRangeFromIndex(int index, int64_t l, int64_t u);

  IntVarElement* Add(IntVar* var);
  void Add(const std::vector<IntVar*>& vars);
  /// Adds without checking if variable has been previously added.
  IntVarElement* FastAdd(IntVar* var);
  int64_t Min(const IntVar* var) const;
  int64_t Max(const IntVar* var) const;
  int64_t Value(const IntVar* var) const;
  bool Bound(const IntVar* var) const;
  void SetMin(const IntVar* var, int64_t m);
  void SetMax(const IntVar* var, int64_t m);
  void SetRange(const IntVar* var, int64_t l, int64_t u);
  void SetValue(const IntVar* var, int64_t value);

  IntervalVarElement* Add(IntervalVar* var);
  void Add(const std::vector<IntervalVar*>& vars);
  /// Adds without checking if variable has been previously added.
  IntervalVarElement* FastAdd(IntervalVar* var);
  int64_t StartMin(const IntervalVar* var) const;
  int64_t StartMax(const IntervalVar* var) const;
  int64_t StartValue(const IntervalVar* var) const;
  int64_t DurationMin(const IntervalVar* var) const;
  int64_t DurationMax(const IntervalVar* var) const;
  int64_t DurationValue(const IntervalVar* var) const;
  int64_t EndMin(const IntervalVar* var) const;
  int64_t EndMax(const IntervalVar* var) const;
  int64_t EndValue(const IntervalVar* var) const;
  int64_t PerformedMin(const IntervalVar* var) const;
  int64_t PerformedMax(const IntervalVar* var) const;
  int64_t PerformedValue(const IntervalVar* var) const;
  void SetStartMin(const IntervalVar* var, int64_t m);
  void SetStartMax(const IntervalVar* var, int64_t m);
  void SetStartRange(const IntervalVar* var, int64_t mi, int64_t ma);
  void SetStartValue(const IntervalVar* var, int64_t value);
  void SetDurationMin(const IntervalVar* var, int64_t m);
  void SetDurationMax(const IntervalVar* var, int64_t m);
  void SetDurationRange(const IntervalVar* var, int64_t mi, int64_t ma);
  void SetDurationValue(const IntervalVar* var, int64_t value);
  void SetEndMin(const IntervalVar* var, int64_t m);
  void SetEndMax(const IntervalVar* var, int64_t m);
  void SetEndRange(const IntervalVar* var, int64_t mi, int64_t ma);
  void SetEndValue(const IntervalVar* var, int64_t value);
  void SetPerformedMin(const IntervalVar* var, int64_t m);
  void SetPerformedMax(const IntervalVar* var, int64_t m);
  void SetPerformedRange(const IntervalVar* var, int64_t mi, int64_t ma);
  void SetPerformedValue(const IntervalVar* var, int64_t value);

  SequenceVarElement* Add(SequenceVar* var);
  void Add(const std::vector<SequenceVar*>& vars);
  /// Adds without checking if the variable had been previously added.
  SequenceVarElement* FastAdd(SequenceVar* var);
  const std::vector<int>& ForwardSequence(const SequenceVar* var) const;
  const std::vector<int>& BackwardSequence(const SequenceVar* var) const;
  const std::vector<int>& Unperformed(const SequenceVar* var) const;
  void SetSequence(const SequenceVar* var,
                   const std::vector<int>& forward_sequence,
                   const std::vector<int>& backward_sequence,
                   const std::vector<int>& unperformed);
  void SetForwardSequence(const SequenceVar* var,
                          const std::vector<int>& forward_sequence);
  void SetBackwardSequence(const SequenceVar* var,
                           const std::vector<int>& backward_sequence);
  void SetUnperformed(const SequenceVar* var,
                      const std::vector<int>& unperformed);

  void Activate(const IntVar* var);
  void Deactivate(const IntVar* var);
  bool Activated(const IntVar* var) const;

  void Activate(const IntervalVar* var);
  void Deactivate(const IntervalVar* var);
  bool Activated(const IntervalVar* var) const;

  void Activate(const SequenceVar* var);
  void Deactivate(const SequenceVar* var);
  bool Activated(const SequenceVar* var) const;

  void ActivateObjective();
  void DeactivateObjective();
  bool ActivatedObjective() const;
  void ActivateObjectiveFromIndex(int index);
  void DeactivateObjectiveFromIndex(int index);
  bool ActivatedObjectiveFromIndex(int index) const;

  std::string DebugString() const override;

  bool AreAllElementsBound() const;

  bool Contains(const IntVar* var) const;
  bool Contains(const IntervalVar* var) const;
  bool Contains(const SequenceVar* var) const;
  /// Copies the intersection of the two assignments to the current
  /// assignment.
  void CopyIntersection(const Assignment* assignment);
  /// Copies 'assignment' to the current assignment, clearing its previous
  /// content.
  void Copy(const Assignment* assignment);

  // TODO(user): Add element iterators to avoid exposing container class.
  const IntContainer& IntVarContainer() const;
  IntContainer* MutableIntVarContainer();
  const IntervalContainer& IntervalVarContainer() const;
  IntervalContainer* MutableIntervalVarContainer();
  const SequenceContainer& SequenceVarContainer() const;
  SequenceContainer* MutableSequenceVarContainer();
  bool operator==(const Assignment& assignment) const;
  bool operator!=(const Assignment& assignment) const;

 private:
  IntContainer int_var_container_;
  IntervalContainer interval_var_container_;
  SequenceContainer sequence_var_container_;
  std::vector<IntVarElement> objective_elements_;
};

std::ostream& operator<<(std::ostream& out,
                         const Assignment& assignment);  /// NOLINT

/// Given a "source_assignment", clears the "target_assignment" and adds
/// all IntVars in "target_vars", with the values of the variables set according
/// to the corresponding values of "source_vars" in "source_assignment".
/// source_vars and target_vars must have the same number of elements.
/// The source and target assignments can belong to different Solvers.
void SetAssignmentFromAssignment(Assignment* target_assignment,
                                 const std::vector<IntVar*>& target_vars,
                                 const Assignment* source_assignment,
                                 const std::vector<IntVar*>& source_vars);

}  // namespace operations_research

#endif  // ORTOOLS_CONSTRAINT_SOLVER_ASSIGNMENT_H_
