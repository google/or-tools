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

#include <stddef.h>

#include <cstdint>
#include <limits>
#include <ostream>
#include <string>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "absl/container/flat_hash_set.h"
#include "absl/log/check.h"
#include "absl/log/log.h"
#include "absl/strings/str_format.h"
#include "absl/strings/str_join.h"
#include "absl/strings/string_view.h"
#include "ortools/base/file.h"
#include "ortools/base/map_util.h"
#include "ortools/base/options.h"
#include "ortools/base/recordio.h"
#include "ortools/constraint_solver/assignment.pb.h"
#include "ortools/constraint_solver/constraint_solver.h"

namespace operations_research {

// ----------------- BaseAssignmentContainer ------------------------

int BaseAssignmentContainer::FindWithDefault(const void* var,
                                             int default_value) const {
  auto it = var_to_index_.find(var);
  return it == var_to_index_.end() ? default_value : it->second;
}

int BaseAssignmentContainer::MapSize() const { return var_to_index_.size(); }
bool BaseAssignmentContainer::MapEmpty() const { return var_to_index_.empty(); }
void BaseAssignmentContainer::ClearMap() { var_to_index_.clear(); }
void BaseAssignmentContainer::AssignMap(const void* var, int index) const {
  var_to_index_[var] = index;
}
bool BaseAssignmentContainer::FindCopy(const void* var, int* index) const {
  auto it = var_to_index_.find(var);
  if (it == var_to_index_.end()) {
    return false;
  }
  *index = it->second;
  return true;
}

template class AssignmentContainer<IntVar, IntVarElement>;
template class AssignmentContainer<IntervalVar, IntervalVarElement>;
template class AssignmentContainer<SequenceVar, SequenceVarElement>;

// ----- IntVarElement -----

IntVarElement::IntVarElement() { Reset(nullptr); }

IntVarElement::IntVarElement(IntVar* var) { Reset(var); }

void IntVarElement::Reset(IntVar* var) {
  var_ = var;
  min_ = std::numeric_limits<int64_t>::min();
  max_ = std::numeric_limits<int64_t>::max();
}

IntVarElement* IntVarElement::Clone() {
  IntVarElement* element = new IntVarElement;
  element->Copy(*this);
  return element;
}

void IntVarElement::Copy(const IntVarElement& element) {
  SetRange(element.min_, element.max_);
  var_ = element.var_;
  if (element.Activated()) {
    Activate();
  } else {
    Deactivate();
  }
}

void IntVarElement::LoadFromProto(
    const IntVarAssignment& int_var_assignment_proto) {
  min_ = int_var_assignment_proto.min();
  max_ = int_var_assignment_proto.max();
  if (int_var_assignment_proto.active()) {
    Activate();
  } else {
    Deactivate();
  }
}

bool IntVarElement::operator==(const IntVarElement& element) const {
  if (var_ != element.var_) {
    return false;
  }
  if (Activated() != element.Activated()) {
    return false;
  }
  if (!Activated() && !element.Activated()) {
    // If both elements are deactivated, then they are equal, regardless of
    // their min and max.
    return true;
  }
  return min_ == element.min_ && max_ == element.max_;
}

void IntVarElement::WriteToProto(
    IntVarAssignment* int_var_assignment_proto) const {
  int_var_assignment_proto->set_var_id(var_->name());
  int_var_assignment_proto->set_min(min_);
  int_var_assignment_proto->set_max(max_);
  int_var_assignment_proto->set_active(Activated());
}

std::string IntVarElement::DebugString() const {
  if (Activated()) {
    if (min_ == max_) {
      return absl::StrFormat("(%d)", min_);
    } else {
      return absl::StrFormat("(%d..%d)", min_, max_);
    }
  } else {
    return "(...)";
  }
}

// ----- IntervalVarElement -----

IntervalVarElement::IntervalVarElement() { Reset(nullptr); }

IntervalVarElement::IntervalVarElement(IntervalVar* var) { Reset(var); }

void IntervalVarElement::Reset(IntervalVar* var) {
  var_ = var;
  start_min_ = std::numeric_limits<int64_t>::min();
  start_max_ = std::numeric_limits<int64_t>::max();
  duration_min_ = std::numeric_limits<int64_t>::min();
  duration_max_ = std::numeric_limits<int64_t>::max();
  end_min_ = std::numeric_limits<int64_t>::min();
  end_max_ = std::numeric_limits<int64_t>::max();
  performed_min_ = 0;
  performed_max_ = 1;
}

IntervalVarElement* IntervalVarElement::Clone() {
  IntervalVarElement* element = new IntervalVarElement;
  element->Copy(*this);
  return element;
}

void IntervalVarElement::Copy(const IntervalVarElement& element) {
  SetStartRange(element.start_min_, element.start_max_);
  SetDurationRange(element.duration_min_, element.duration_max_);
  SetEndRange(element.end_min_, element.end_max_);
  SetPerformedRange(element.performed_min_, element.performed_max_);
  var_ = element.var_;
  if (element.Activated()) {
    Activate();
  } else {
    Deactivate();
  }
}

void IntervalVarElement::Store() {
  performed_min_ = static_cast<int64_t>(var_->MustBePerformed());
  performed_max_ = static_cast<int64_t>(var_->MayBePerformed());
  if (performed_max_ != 0LL) {
    start_min_ = var_->StartMin();
    start_max_ = var_->StartMax();
    duration_min_ = var_->DurationMin();
    duration_max_ = var_->DurationMax();
    end_min_ = var_->EndMin();
    end_max_ = var_->EndMax();
  }
}

void IntervalVarElement::Restore() {
  if (performed_max_ == performed_min_) {
    var_->SetPerformed(performed_min_);
  }
  if (performed_max_ != 0LL) {
    var_->SetStartRange(start_min_, start_max_);
    var_->SetDurationRange(duration_min_, duration_max_);
    var_->SetEndRange(end_min_, end_max_);
  }
}

void IntervalVarElement::LoadFromProto(
    const IntervalVarAssignment& interval_var_assignment_proto) {
  start_min_ = interval_var_assignment_proto.start_min();
  start_max_ = interval_var_assignment_proto.start_max();
  duration_min_ = interval_var_assignment_proto.duration_min();
  duration_max_ = interval_var_assignment_proto.duration_max();
  end_min_ = interval_var_assignment_proto.end_min();
  end_max_ = interval_var_assignment_proto.end_max();
  performed_min_ = interval_var_assignment_proto.performed_min();
  performed_max_ = interval_var_assignment_proto.performed_max();
  if (interval_var_assignment_proto.active()) {
    Activate();
  } else {
    Deactivate();
  }
}

void IntervalVarElement::WriteToProto(
    IntervalVarAssignment* interval_var_assignment_proto) const {
  interval_var_assignment_proto->set_var_id(var_->name());
  interval_var_assignment_proto->set_start_min(start_min_);
  interval_var_assignment_proto->set_start_max(start_max_);
  interval_var_assignment_proto->set_duration_min(duration_min_);
  interval_var_assignment_proto->set_duration_max(duration_max_);
  interval_var_assignment_proto->set_end_min(end_min_);
  interval_var_assignment_proto->set_end_max(end_max_);
  interval_var_assignment_proto->set_performed_min(performed_min_);
  interval_var_assignment_proto->set_performed_max(performed_max_);
  interval_var_assignment_proto->set_active(Activated());
}

std::string IntervalVarElement::DebugString() const {
  if (Activated()) {
    std::string out;
    absl::StrAppendFormat(&out, "(start = %d", start_min_);
    if (start_max_ != start_min_) {
      absl::StrAppendFormat(&out, "..%d", start_max_);
    }
    absl::StrAppendFormat(&out, ", duration = %d", duration_min_);
    if (duration_max_ != duration_min_) {
      absl::StrAppendFormat(&out, "..%d", duration_max_);
    }
    absl::StrAppendFormat(&out, ", status = %d", performed_min_);
    if (performed_max_ != performed_min_) {
      absl::StrAppendFormat(&out, "..%d", performed_max_);
    }
    out.append(")");
    return out;
  } else {
    return "(...)";
  }
}

bool IntervalVarElement::operator==(const IntervalVarElement& element) const {
  if (var_ != element.var_) {
    return false;
  }
  if (Activated() != element.Activated()) {
    return false;
  }
  if (!Activated() && !element.Activated()) {
    // If both elements are deactivated, then they are equal, regardless of
    // their other fields.
    return true;
  }
  return start_min_ == element.start_min_ && start_max_ == element.start_max_ &&
         duration_min_ == element.duration_min_ &&
         duration_max_ == element.duration_max_ &&
         end_min_ == element.end_min_ && end_max_ == element.end_max_ &&
         performed_min_ == element.performed_min_ &&
         performed_max_ == element.performed_max_ && var_ == element.var_;
}

// ----- SequenceVarElement -----

SequenceVarElement::SequenceVarElement() { Reset(nullptr); }

SequenceVarElement::SequenceVarElement(SequenceVar* var) { Reset(var); }

void SequenceVarElement::Reset(SequenceVar* var) {
  var_ = var;
  forward_sequence_.clear();
  backward_sequence_.clear();
  unperformed_.clear();
}

SequenceVarElement* SequenceVarElement::Clone() {
  SequenceVarElement* const element = new SequenceVarElement;
  element->Copy(*this);
  return element;
}

void SequenceVarElement::Copy(const SequenceVarElement& element) {
  forward_sequence_ = element.forward_sequence_;
  backward_sequence_ = element.backward_sequence_;
  unperformed_ = element.unperformed_;
  var_ = element.var_;
  if (element.Activated()) {
    Activate();
  } else {
    Deactivate();
  }
}

void SequenceVarElement::Store() {
  var_->FillSequence(&forward_sequence_, &backward_sequence_, &unperformed_);
}

void SequenceVarElement::Restore() {
  var_->RankSequence(forward_sequence_, backward_sequence_, unperformed_);
}

void SequenceVarElement::LoadFromProto(
    const SequenceVarAssignment& sequence_var_assignment_proto) {
  for (const int32_t forward_sequence :
       sequence_var_assignment_proto.forward_sequence()) {
    forward_sequence_.push_back(forward_sequence);
  }
  for (const int32_t backward_sequence :
       sequence_var_assignment_proto.backward_sequence()) {
    backward_sequence_.push_back(backward_sequence);
  }
  for (const int32_t unperformed :
       sequence_var_assignment_proto.unperformed()) {
    unperformed_.push_back(unperformed);
  }
  if (sequence_var_assignment_proto.active()) {
    Activate();
  } else {
    Deactivate();
  }
  DCHECK(CheckClassInvariants());
}

void SequenceVarElement::WriteToProto(
    SequenceVarAssignment* sequence_var_assignment_proto) const {
  sequence_var_assignment_proto->set_var_id(var_->name());
  sequence_var_assignment_proto->set_active(Activated());
  for (const int forward_sequence : forward_sequence_) {
    sequence_var_assignment_proto->add_forward_sequence(forward_sequence);
  }
  for (const int backward_sequence : backward_sequence_) {
    sequence_var_assignment_proto->add_backward_sequence(backward_sequence);
  }
  for (const int unperformed : unperformed_) {
    sequence_var_assignment_proto->add_unperformed(unperformed);
  }
}

std::string SequenceVarElement::DebugString() const {
  if (Activated()) {
    return absl::StrFormat("[forward %s, backward %s, unperformed [%s]]",
                           absl::StrJoin(forward_sequence_, " -> "),
                           absl::StrJoin(backward_sequence_, " -> "),
                           absl::StrJoin(unperformed_, ", "));
  } else {
    return "(...)";
  }
}

bool SequenceVarElement::operator==(const SequenceVarElement& element) const {
  if (var_ != element.var_) {
    return false;
  }
  if (Activated() != element.Activated()) {
    return false;
  }
  if (!Activated() && !element.Activated()) {
    // If both elements are deactivated, then they are equal, regardless of
    // their other fields.
    return true;
  }
  return forward_sequence_ == element.forward_sequence_ &&
         backward_sequence_ == element.backward_sequence_ &&
         unperformed_ == element.unperformed_;
}

const std::vector<int>& SequenceVarElement::ForwardSequence() const {
  return forward_sequence_;
}

const std::vector<int>& SequenceVarElement::BackwardSequence() const {
  return backward_sequence_;
}

const std::vector<int>& SequenceVarElement::Unperformed() const {
  return unperformed_;
}

void SequenceVarElement::SetSequence(const std::vector<int>& forward_sequence,
                                     const std::vector<int>& backward_sequence,
                                     const std::vector<int>& unperformed) {
  forward_sequence_ = forward_sequence;
  backward_sequence_ = backward_sequence;
  unperformed_ = unperformed;
  DCHECK(CheckClassInvariants());
}

void SequenceVarElement::SetForwardSequence(
    const std::vector<int>& forward_sequence) {
  forward_sequence_ = forward_sequence;
}

void SequenceVarElement::SetBackwardSequence(
    const std::vector<int>& backward_sequence) {
  backward_sequence_ = backward_sequence;
}

void SequenceVarElement::SetUnperformed(const std::vector<int>& unperformed) {
  unperformed_ = unperformed;
}

bool SequenceVarElement::CheckClassInvariants() {
  absl::flat_hash_set<int> visited;
  for (const int forward_sequence : forward_sequence_) {
    if (visited.contains(forward_sequence)) {
      return false;
    }
    visited.insert(forward_sequence);
  }
  for (const int backward_sequence : backward_sequence_) {
    if (visited.contains(backward_sequence)) {
      return false;
    }
    visited.insert(backward_sequence);
  }
  for (const int unperformed : unperformed_) {
    if (visited.contains(unperformed)) {
      return false;
    }
    visited.insert(unperformed);
  }
  return true;
}

// ----- Assignment -----

Assignment::Assignment(const Assignment* const copy)
    : PropagationBaseObject(copy->solver()),
      int_var_container_(copy->int_var_container_),
      interval_var_container_(copy->interval_var_container_),
      sequence_var_container_(copy->sequence_var_container_),
      objective_elements_(copy->objective_elements_) {}

Assignment::Assignment(Solver* solver) : PropagationBaseObject(solver) {}
Assignment::~Assignment() {}

void Assignment::Clear() {
  objective_elements_.clear();
  int_var_container_.Clear();
  interval_var_container_.Clear();
  sequence_var_container_.Clear();
}

void Assignment::Store() {
  int_var_container_.Store();
  interval_var_container_.Store();
  sequence_var_container_.Store();
  for (IntVarElement& objective_element : objective_elements_) {
    objective_element.Store();
  }
}

void Assignment::Restore() {
  FreezeQueue();
  int_var_container_.Restore();
  interval_var_container_.Restore();
  sequence_var_container_.Restore();
  UnfreezeQueue();
}

namespace {

template <class V, class E>
void IdToElementMap(AssignmentContainer<V, E>* container,
                    absl::flat_hash_map<std::string, E*>* id_to_element_map) {
  CHECK(id_to_element_map != nullptr);
  id_to_element_map->clear();
  for (int i = 0; i < container->Size(); ++i) {
    E* const element = container->MutableElement(i);
    const V* const var = element->Var();
    const std::string& name = var->name();
    if (name.empty()) {
      LOG(INFO) << "Cannot save/load variables with empty name"
                << "; variable will be ignored";
    } else if (id_to_element_map->contains(name)) {
      LOG(INFO) << "Cannot save/load variables with duplicate names: " << name
                << "; variable will be ignored";
    } else {
      (*id_to_element_map)[name] = element;
    }
  }
}

template <class E, class P>
void LoadElement(const absl::flat_hash_map<std::string, E*>& id_to_element_map,
                 const P& proto) {
  absl::string_view var_id = proto.var_id();
  CHECK(!var_id.empty());
  E* element = nullptr;
  if (gtl::FindCopy(id_to_element_map, var_id, &element)) {
    element->LoadFromProto(proto);
  } else {
    LOG(INFO) << "Variable " << var_id
              << " not in assignment; skipping variable";
  }
}

}  // namespace

bool Assignment::Load(const std::string& filename) {
  File* file;
  if (!file::Open(filename, "r", &file, file::Defaults()).ok()) {
    LOG(INFO) << "Cannot open " << filename;
    return false;
  }
  return Load(file);
}

bool Assignment::Load(File* file) {
  CHECK(file != nullptr);
  AssignmentProto assignment_proto;
  recordio::RecordReader reader(file);
  if (!reader.ReadProtocolMessage(&assignment_proto)) {
    LOG(INFO) << "No assignment found in " << file->filename();
    return false;
  }
  Load(assignment_proto);
  return reader.Close();
}

template <class Var, class Element, class Proto, class Container>
void RealLoad(const AssignmentProto& assignment_proto,
              Container* const container,
              int (AssignmentProto::*GetSize)() const,
              const Proto& (AssignmentProto::*GetElem)(int) const) {
  bool fast_load = (container->Size() == (assignment_proto.*GetSize)());
  for (int i = 0; fast_load && i < (assignment_proto.*GetSize)(); ++i) {
    Element* const element = container->MutableElement(i);
    const Proto& proto = (assignment_proto.*GetElem)(i);
    if (element->Var()->name() == proto.var_id()) {
      element->LoadFromProto(proto);
    } else {
      fast_load = false;
    }
  }
  if (!fast_load) {
    absl::flat_hash_map<std::string, Element*> id_to_element_map;
    IdToElementMap<Var, Element>(container, &id_to_element_map);
    for (int i = 0; i < (assignment_proto.*GetSize)(); ++i) {
      LoadElement<Element, Proto>(id_to_element_map,
                                  (assignment_proto.*GetElem)(i));
    }
  }
}

void Assignment::Load(const AssignmentProto& assignment_proto) {
  RealLoad<IntVar, IntVarElement, IntVarAssignment, IntContainer>(
      assignment_proto, &int_var_container_,
      &AssignmentProto::int_var_assignment_size,
      &AssignmentProto::int_var_assignment);
  RealLoad<IntervalVar, IntervalVarElement, IntervalVarAssignment,
           IntervalContainer>(assignment_proto, &interval_var_container_,
                              &AssignmentProto::interval_var_assignment_size,
                              &AssignmentProto::interval_var_assignment);
  RealLoad<SequenceVar, SequenceVarElement, SequenceVarAssignment,
           SequenceContainer>(assignment_proto, &sequence_var_container_,
                              &AssignmentProto::sequence_var_assignment_size,
                              &AssignmentProto::sequence_var_assignment);
  for (int i = 0; i < assignment_proto.objective_size(); ++i) {
    const IntVarAssignment& objective = assignment_proto.objective(i);
    absl::string_view objective_id = objective.var_id();
    DCHECK(!objective_id.empty());
    if (HasObjectiveFromIndex(i) &&
        objective_id == ObjectiveFromIndex(i)->name()) {
      const int64_t obj_min = objective.min();
      const int64_t obj_max = objective.max();
      SetObjectiveRangeFromIndex(i, obj_min, obj_max);
      if (objective.active()) {
        ActivateObjectiveFromIndex(i);
      } else {
        DeactivateObjectiveFromIndex(i);
      }
    }
  }
}

bool Assignment::Save(const std::string& filename) const {
  File* file;
  if (!file::Open(filename, "w", &file, file::Defaults()).ok()) {
    LOG(INFO) << "Cannot open " << filename;
    return false;
  }
  return Save(file);
}

bool Assignment::Save(File* file) const {
  CHECK(file != nullptr);
  AssignmentProto assignment_proto;
  Save(&assignment_proto);
  recordio::RecordWriter writer(file);
  return writer.WriteProtocolMessage(assignment_proto) && writer.Close();
}

template <class Var, class Element, class Proto, class Container>
void RealSave(AssignmentProto* const assignment_proto,
              const Container& container, Proto* (AssignmentProto::*Add)()) {
  for (const Element& element : container.elements()) {
    const Var* const var = element.Var();
    const std::string& name = var->name();
    if (!name.empty()) {
      Proto* const var_assignment_proto = (assignment_proto->*Add)();
      element.WriteToProto(var_assignment_proto);
    }
  }
}

void Assignment::Save(AssignmentProto* const assignment_proto) const {
  assignment_proto->Clear();
  RealSave<IntVar, IntVarElement, IntVarAssignment, IntContainer>(
      assignment_proto, int_var_container_,
      &AssignmentProto::add_int_var_assignment);
  RealSave<IntervalVar, IntervalVarElement, IntervalVarAssignment,
           IntervalContainer>(assignment_proto, interval_var_container_,
                              &AssignmentProto::add_interval_var_assignment);
  RealSave<SequenceVar, SequenceVarElement, SequenceVarAssignment,
           SequenceContainer>(assignment_proto, sequence_var_container_,
                              &AssignmentProto::add_sequence_var_assignment);
  for (int i = 0; i < objective_elements_.size(); ++i) {
    const std::string& name = ObjectiveFromIndex(i)->name();
    if (!name.empty()) {
      IntVarAssignment* objective = assignment_proto->add_objective();
      objective->set_var_id(name);
      objective->set_min(ObjectiveMinFromIndex(i));
      objective->set_max(ObjectiveMaxFromIndex(i));
      objective->set_active(ActivatedObjectiveFromIndex(i));
    }
  }
}

template <class Container, class Element>
void RealDebugString(const Container& container, std::string* const out) {
  for (const Element& element : container.elements()) {
    if (element.Var() != nullptr) {
      absl::StrAppendFormat(out, "%s %s | ", element.Var()->name(),
                            element.DebugString());
    }
  }
}

std::string Assignment::DebugString() const {
  std::string out = "Assignment(";
  RealDebugString<IntContainer, IntVarElement>(int_var_container_, &out);
  RealDebugString<IntervalContainer, IntervalVarElement>(
      interval_var_container_, &out);
  RealDebugString<SequenceContainer, SequenceVarElement>(
      sequence_var_container_, &out);
  std::vector<std::string> objective_str;
  for (const IntVarElement& objective_element : objective_elements_) {
    if (objective_element.Activated()) {
      objective_str.push_back(objective_element.DebugString());
    }
  }
  absl::StrAppendFormat(&out, "%s)", absl::StrJoin(objective_str, ", "));
  return out;
}

IntVarElement* Assignment::Add(IntVar* var) {
  return int_var_container_.Add(var);
}

void Assignment::Add(const std::vector<IntVar*>& vars) {
  for (IntVar* var : vars) {
    Add(var);
  }
}

IntVarElement* Assignment::FastAdd(IntVar* var) {
  return int_var_container_.FastAdd(var);
}

int64_t Assignment::Min(const IntVar* var) const {
  return int_var_container_.Element(var).Min();
}

int64_t Assignment::Max(const IntVar* var) const {
  return int_var_container_.Element(var).Max();
}

int64_t Assignment::Value(const IntVar* var) const {
  return int_var_container_.Element(var).Value();
}

bool Assignment::Bound(const IntVar* var) const {
  return int_var_container_.Element(var).Bound();
}

void Assignment::SetMin(const IntVar* var, int64_t m) {
  int_var_container_.MutableElement(var)->SetMin(m);
}

void Assignment::SetMax(const IntVar* var, int64_t m) {
  int_var_container_.MutableElement(var)->SetMax(m);
}

void Assignment::SetRange(const IntVar* var, int64_t l, int64_t u) {
  int_var_container_.MutableElement(var)->SetRange(l, u);
}

void Assignment::SetValue(const IntVar* var, int64_t value) {
  int_var_container_.MutableElement(var)->SetValue(value);
}

// ----- Interval Var -----

IntervalVarElement* Assignment::Add(IntervalVar* var) {
  return interval_var_container_.Add(var);
}

void Assignment::Add(const std::vector<IntervalVar*>& vars) {
  for (IntervalVar* var : vars) {
    Add(var);
  }
}

IntervalVarElement* Assignment::FastAdd(IntervalVar* var) {
  return interval_var_container_.FastAdd(var);
}

int64_t Assignment::StartMin(const IntervalVar* var) const {
  return interval_var_container_.Element(var).StartMin();
}

int64_t Assignment::StartMax(const IntervalVar* var) const {
  return interval_var_container_.Element(var).StartMax();
}

int64_t Assignment::StartValue(const IntervalVar* var) const {
  return interval_var_container_.Element(var).StartValue();
}

int64_t Assignment::DurationMin(const IntervalVar* var) const {
  return interval_var_container_.Element(var).DurationMin();
}

int64_t Assignment::DurationMax(const IntervalVar* var) const {
  return interval_var_container_.Element(var).DurationMax();
}

int64_t Assignment::DurationValue(const IntervalVar* var) const {
  return interval_var_container_.Element(var).DurationValue();
}

int64_t Assignment::EndMin(const IntervalVar* var) const {
  return interval_var_container_.Element(var).EndMin();
}

int64_t Assignment::EndMax(const IntervalVar* var) const {
  return interval_var_container_.Element(var).EndMax();
}

int64_t Assignment::EndValue(const IntervalVar* var) const {
  return interval_var_container_.Element(var).EndValue();
}

int64_t Assignment::PerformedMin(const IntervalVar* var) const {
  return interval_var_container_.Element(var).PerformedMin();
}

int64_t Assignment::PerformedMax(const IntervalVar* var) const {
  return interval_var_container_.Element(var).PerformedMax();
}

int64_t Assignment::PerformedValue(const IntervalVar* var) const {
  return interval_var_container_.Element(var).PerformedValue();
}

void Assignment::SetStartMin(const IntervalVar* var, int64_t m) {
  interval_var_container_.MutableElement(var)->SetStartMin(m);
}

void Assignment::SetStartMax(const IntervalVar* var, int64_t m) {
  interval_var_container_.MutableElement(var)->SetStartMax(m);
}

void Assignment::SetStartRange(const IntervalVar* var, int64_t mi, int64_t ma) {
  interval_var_container_.MutableElement(var)->SetStartRange(mi, ma);
}

void Assignment::SetStartValue(const IntervalVar* var, int64_t value) {
  interval_var_container_.MutableElement(var)->SetStartValue(value);
}

void Assignment::SetDurationMin(const IntervalVar* var, int64_t m) {
  interval_var_container_.MutableElement(var)->SetDurationMin(m);
}

void Assignment::SetDurationMax(const IntervalVar* var, int64_t m) {
  interval_var_container_.MutableElement(var)->SetDurationMax(m);
}

void Assignment::SetDurationRange(const IntervalVar* var, int64_t mi,
                                  int64_t ma) {
  interval_var_container_.MutableElement(var)->SetDurationRange(mi, ma);
}

void Assignment::SetDurationValue(const IntervalVar* var, int64_t value) {
  interval_var_container_.MutableElement(var)->SetDurationValue(value);
}

void Assignment::SetEndMin(const IntervalVar* var, int64_t m) {
  interval_var_container_.MutableElement(var)->SetEndMin(m);
}

void Assignment::SetEndMax(const IntervalVar* var, int64_t m) {
  interval_var_container_.MutableElement(var)->SetEndMax(m);
}

void Assignment::SetEndRange(const IntervalVar* var, int64_t mi, int64_t ma) {
  interval_var_container_.MutableElement(var)->SetEndRange(mi, ma);
}

void Assignment::SetEndValue(const IntervalVar* var, int64_t value) {
  interval_var_container_.MutableElement(var)->SetEndValue(value);
}

void Assignment::SetPerformedMin(const IntervalVar* var, int64_t m) {
  interval_var_container_.MutableElement(var)->SetPerformedMin(m);
}

void Assignment::SetPerformedMax(const IntervalVar* var, int64_t m) {
  interval_var_container_.MutableElement(var)->SetPerformedMax(m);
}

void Assignment::SetPerformedRange(const IntervalVar* var, int64_t mi,
                                   int64_t ma) {
  interval_var_container_.MutableElement(var)->SetPerformedRange(mi, ma);
}

void Assignment::SetPerformedValue(const IntervalVar* var, int64_t value) {
  interval_var_container_.MutableElement(var)->SetPerformedValue(value);
}

// ----- Sequence Var -----

SequenceVarElement* Assignment::Add(SequenceVar* var) {
  return sequence_var_container_.Add(var);
}

void Assignment::Add(const std::vector<SequenceVar*>& vars) {
  for (SequenceVar* var : vars) {
    Add(var);
  }
}

SequenceVarElement* Assignment::FastAdd(SequenceVar* var) {
  return sequence_var_container_.FastAdd(var);
}

const std::vector<int>& Assignment::ForwardSequence(
    const SequenceVar* var) const {
  return sequence_var_container_.Element(var).ForwardSequence();
}

const std::vector<int>& Assignment::BackwardSequence(
    const SequenceVar* var) const {
  return sequence_var_container_.Element(var).BackwardSequence();
}

const std::vector<int>& Assignment::Unperformed(const SequenceVar* var) const {
  return sequence_var_container_.Element(var).Unperformed();
}

void Assignment::SetSequence(const SequenceVar* var,
                             const std::vector<int>& forward_sequence,
                             const std::vector<int>& backward_sequence,
                             const std::vector<int>& unperformed) {
  sequence_var_container_.MutableElement(var)->SetSequence(
      forward_sequence, backward_sequence, unperformed);
}

void Assignment::SetForwardSequence(const SequenceVar* var,
                                    const std::vector<int>& forward_sequence) {
  sequence_var_container_.MutableElement(var)->SetForwardSequence(
      forward_sequence);
}

void Assignment::SetBackwardSequence(
    const SequenceVar* var, const std::vector<int>& backward_sequence) {
  sequence_var_container_.MutableElement(var)->SetBackwardSequence(
      backward_sequence);
}

void Assignment::SetUnperformed(const SequenceVar* var,
                                const std::vector<int>& unperformed) {
  sequence_var_container_.MutableElement(var)->SetUnperformed(unperformed);
}

void Assignment::Activate(const IntVar* var) {
  int_var_container_.MutableElement(var)->Activate();
}

void Assignment::Deactivate(const IntVar* var) {
  int_var_container_.MutableElement(var)->Deactivate();
}

bool Assignment::Activated(const IntVar* var) const {
  return int_var_container_.Element(var).Activated();
}

void Assignment::Activate(const IntervalVar* var) {
  interval_var_container_.MutableElement(var)->Activate();
}

void Assignment::Deactivate(const IntervalVar* var) {
  interval_var_container_.MutableElement(var)->Deactivate();
}

bool Assignment::Activated(const IntervalVar* var) const {
  return interval_var_container_.Element(var).Activated();
}

void Assignment::Activate(const SequenceVar* var) {
  sequence_var_container_.MutableElement(var)->Activate();
}

void Assignment::Deactivate(const SequenceVar* var) {
  sequence_var_container_.MutableElement(var)->Deactivate();
}

bool Assignment::Activated(const SequenceVar* var) const {
  return sequence_var_container_.Element(var).Activated();
}

bool Assignment::Contains(const IntVar* var) const {
  return int_var_container_.Contains(var);
}

bool Assignment::Contains(const IntervalVar* var) const {
  return interval_var_container_.Contains(var);
}

bool Assignment::Contains(const SequenceVar* var) const {
  return sequence_var_container_.Contains(var);
}

void Assignment::CopyIntersection(const Assignment* assignment) {
  int_var_container_.CopyIntersection(assignment->int_var_container_);
  interval_var_container_.CopyIntersection(assignment->interval_var_container_);
  sequence_var_container_.CopyIntersection(assignment->sequence_var_container_);
  for (int i = 0; i < objective_elements_.size(); i++) {
    if (i >= assignment->objective_elements_.size() ||
        // TODO(user): The current behavior is to copy the objective "prefix"
        // which fits the notion of lexicographic objectives well. Reconsider if
        // multiple objectives are used in another context.
        objective_elements_[i].Var() !=
            assignment->objective_elements_[i].Var()) {
      break;
    }
    objective_elements_[i] = assignment->objective_elements_[i];
  }
}

void Assignment::Copy(const Assignment* assignment) {
  Clear();
  int_var_container_.Copy(assignment->int_var_container_);
  interval_var_container_.Copy(assignment->interval_var_container_);
  sequence_var_container_.Copy(assignment->sequence_var_container_);
  objective_elements_ = assignment->objective_elements_;
}

void SetAssignmentFromAssignment(Assignment* target_assignment,
                                 const std::vector<IntVar*>& target_vars,
                                 const Assignment* source_assignment,
                                 const std::vector<IntVar*>& source_vars) {
  const int vars_size = target_vars.size();
  CHECK_EQ(source_vars.size(), vars_size);
  CHECK(target_assignment != nullptr);
  CHECK(source_assignment != nullptr);

  target_assignment->Clear();
  const Solver* const target_solver = target_assignment->solver();
  const Solver* const source_solver = source_assignment->solver();
  for (int index = 0; index < vars_size; index++) {
    IntVar* target_var = target_vars[index];
    CHECK_EQ(target_var->solver(), target_solver);
    IntVar* source_var = source_vars[index];
    CHECK_EQ(source_var->solver(), source_solver);
    target_assignment->Add(target_var)
        ->SetValue(source_assignment->Value(source_var));
  }
}

Assignment* Solver::MakeAssignment() { return RevAlloc(new Assignment(this)); }

Assignment* Solver::MakeAssignment(const Assignment* const a) {
  return RevAlloc(new Assignment(a));
}

// ----- Storing and Restoring assignments -----
namespace {
class RestoreAssignment : public DecisionBuilder {
 public:
  explicit RestoreAssignment(Assignment* assignment)
      : assignment_(assignment) {}

  ~RestoreAssignment() override {}

  Decision* Next(Solver* const /*solver*/) override {
    assignment_->Restore();
    return nullptr;
  }

  std::string DebugString() const override { return "RestoreAssignment"; }

 private:
  Assignment* const assignment_;
};

class StoreAssignment : public DecisionBuilder {
 public:
  explicit StoreAssignment(Assignment* assignment) : assignment_(assignment) {}

  ~StoreAssignment() override {}

  Decision* Next(Solver* const /*solver*/) override {
    assignment_->Store();
    return nullptr;
  }

  std::string DebugString() const override { return "StoreAssignment"; }

 private:
  Assignment* const assignment_;
};
}  // namespace

DecisionBuilder* Solver::MakeRestoreAssignment(Assignment* assignment) {
  return RevAlloc(new RestoreAssignment(assignment));
}

DecisionBuilder* Solver::MakeStoreAssignment(Assignment* assignment) {
  return RevAlloc(new StoreAssignment(assignment));
}

bool Assignment::Empty() const {
  return int_var_container_.Empty() && interval_var_container_.Empty() &&
         sequence_var_container_.Empty();
}

int Assignment::Size() const {
  return NumIntVars() + NumIntervalVars() + NumSequenceVars();
}

int Assignment::NumIntVars() const { return int_var_container_.Size(); }

int Assignment::NumIntervalVars() const {
  return interval_var_container_.Size();
}

int Assignment::NumSequenceVars() const {
  return sequence_var_container_.Size();
}

void Assignment::AddObjective(IntVar* v) { AddObjectives({v}); }

void Assignment::AddObjectives(const std::vector<IntVar*>& vars) {
  // Objective can only set once.
  DCHECK(!HasObjective());
  objective_elements_.reserve(vars.size());
  for (IntVar* var : vars) {
    if (var != nullptr) {
      objective_elements_.emplace_back(var);
    }
  }
}

void Assignment::ClearObjective() { objective_elements_.clear(); }

int Assignment::NumObjectives() const { return objective_elements_.size(); }

IntVar* Assignment::Objective() const { return ObjectiveFromIndex(0); }

IntVar* Assignment::ObjectiveFromIndex(int index) const {
  return HasObjectiveFromIndex(index) ? objective_elements_[index].Var()
                                      : nullptr;
}

bool Assignment::HasObjective() const { return !objective_elements_.empty(); }

bool Assignment::HasObjectiveFromIndex(int index) const {
  return index < objective_elements_.size();
}

int64_t Assignment::ObjectiveMin() const { return ObjectiveMinFromIndex(0); }

int64_t Assignment::ObjectiveMax() const { return ObjectiveMaxFromIndex(0); }

int64_t Assignment::ObjectiveValue() const {
  return ObjectiveValueFromIndex(0);
}

bool Assignment::ObjectiveBound() const { return ObjectiveBoundFromIndex(0); }

void Assignment::SetObjectiveMin(int64_t m) { SetObjectiveMinFromIndex(0, m); }

void Assignment::SetObjectiveMax(int64_t m) { SetObjectiveMaxFromIndex(0, m); }

void Assignment::SetObjectiveValue(int64_t value) {
  SetObjectiveValueFromIndex(0, value);
}

void Assignment::SetObjectiveRange(int64_t l, int64_t u) {
  SetObjectiveRangeFromIndex(0, l, u);
}

int64_t Assignment::ObjectiveMinFromIndex(int index) const {
  return HasObjectiveFromIndex(index) ? objective_elements_[index].Min() : 0;
}

int64_t Assignment::ObjectiveMaxFromIndex(int index) const {
  return HasObjectiveFromIndex(index) ? objective_elements_[index].Max() : 0;
}

int64_t Assignment::ObjectiveValueFromIndex(int index) const {
  return HasObjectiveFromIndex(index) ? objective_elements_[index].Value() : 0;
}

bool Assignment::ObjectiveBoundFromIndex(int index) const {
  return HasObjectiveFromIndex(index) ? objective_elements_[index].Bound()
                                      : true;
}

void Assignment::SetObjectiveMinFromIndex(int index, int64_t m) {
  if (HasObjectiveFromIndex(index)) {
    objective_elements_[index].SetMin(m);
  }
}

void Assignment::SetObjectiveMaxFromIndex(int index, int64_t m) {
  if (HasObjectiveFromIndex(index)) {
    objective_elements_[index].SetMax(m);
  }
}

void Assignment::SetObjectiveValueFromIndex(int index, int64_t value) {
  if (HasObjectiveFromIndex(index)) {
    objective_elements_[index].SetValue(value);
  }
}

void Assignment::SetObjectiveRangeFromIndex(int index, int64_t l, int64_t u) {
  if (HasObjectiveFromIndex(index)) {
    objective_elements_[index].SetRange(l, u);
  }
}

void Assignment::ActivateObjective() { ActivateObjectiveFromIndex(0); }

void Assignment::DeactivateObjective() { DeactivateObjectiveFromIndex(0); }

bool Assignment::ActivatedObjective() const {
  return ActivatedObjectiveFromIndex(0);
}

void Assignment::ActivateObjectiveFromIndex(int index) {
  if (HasObjectiveFromIndex(index)) {
    objective_elements_[index].Activate();
  }
}

void Assignment::DeactivateObjectiveFromIndex(int index) {
  if (HasObjectiveFromIndex(index)) {
    objective_elements_[index].Deactivate();
  }
}

bool Assignment::ActivatedObjectiveFromIndex(int index) const {
  return HasObjectiveFromIndex(index) ? objective_elements_[index].Activated()
                                      : true;
}

bool Assignment::AreAllElementsBound() const {
  return int_var_container_.AreAllElementsBound() &&
         interval_var_container_.AreAllElementsBound() &&
         sequence_var_container_.AreAllElementsBound();
}

const Assignment::IntContainer& Assignment::IntVarContainer() const {
  return int_var_container_;
}

Assignment::IntContainer* Assignment::MutableIntVarContainer() {
  return &int_var_container_;
}

const Assignment::IntervalContainer& Assignment::IntervalVarContainer() const {
  return interval_var_container_;
}

Assignment::IntervalContainer* Assignment::MutableIntervalVarContainer() {
  return &interval_var_container_;
}

const Assignment::SequenceContainer& Assignment::SequenceVarContainer() const {
  return sequence_var_container_;
}

Assignment::SequenceContainer* Assignment::MutableSequenceVarContainer() {
  return &sequence_var_container_;
}

bool Assignment::operator==(const Assignment& assignment) const {
  return int_var_container_ == assignment.int_var_container_ &&
         interval_var_container_ == assignment.interval_var_container_ &&
         sequence_var_container_ == assignment.sequence_var_container_ &&
         objective_elements_ == assignment.objective_elements_;
}

bool Assignment::operator!=(const Assignment& assignment) const {
  return !(*this == assignment);
}

std::ostream& operator<<(std::ostream& out, const Assignment& assignment) {
  return out << assignment.DebugString();
}

}  // namespace operations_research
