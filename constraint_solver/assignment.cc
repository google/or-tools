// Copyright 2010-2011 Google
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
#include "base/hash.h"
#include <string>
#include <vector>

#include "base/integral_types.h"
#include "base/logging.h"
#include "base/stringprintf.h"
#include "base/file.h"
#include "base/recordio.h"
#include "base/concise_iterator.h"
#include "base/map-util.h"
#include "base/hash.h"
#include "constraint_solver/assignment.pb.h"
#include "constraint_solver/constraint_solver.h"

namespace operations_research {

// ----------------- Solutions ------------------------

// ----- IntVarElement -----

IntVarElement::IntVarElement() {
  Reset(NULL);
}

IntVarElement::IntVarElement(IntVar* const var) {
  Reset(var);
}

void IntVarElement::Reset(IntVar* const var) {
  var_ = var;
  min_ = kint64min;
  max_ = kint64max;
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

void IntVarElement::StoreFromProto(
    const IntVarAssignmentProto& int_var_assignment_proto) {
  min_ = int_var_assignment_proto.min();
  max_ = int_var_assignment_proto.max();
  if (int_var_assignment_proto.active()) {
    Activate();
  } else {
    Deactivate();
  }
}

void IntVarElement::RestoreToProto(
    IntVarAssignmentProto* int_var_assignment_proto) const {
  int_var_assignment_proto->set_var_id(var_->name());
  int_var_assignment_proto->set_min(min_);
  int_var_assignment_proto->set_max(max_);
  int_var_assignment_proto->set_active(Activated());
}

string IntVarElement::DebugString() const {
  if (Activated()) {
    if (min_ == max_) {
      return StringPrintf("(%" GG_LL_FORMAT "d)", min_);
    } else {
      return StringPrintf("(%" GG_LL_FORMAT "d..%" GG_LL_FORMAT "d)",
                          min_, max_);
    }
  } else {
    return "(...)";
  }
}

// ----- IntervalVarElement -----

IntervalVarElement::IntervalVarElement() {
  Reset(NULL);
}

IntervalVarElement::IntervalVarElement(IntervalVar* const var) {
  Reset(var);
}

void IntervalVarElement::Reset(IntervalVar* const var) {
  var_ = var;
  start_min_ = kint64min;
  start_max_ = kint64max;
  duration_min_ = kint64min;
  duration_max_ = kint64max;
  end_min_ = kint64min;
  end_max_ = kint64max;
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
  performed_min_ = static_cast<int64>(var_->MustBePerformed());
  performed_max_ = static_cast<int64>(var_->MayBePerformed());
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

void IntervalVarElement::StoreFromProto(
    const IntervalVarAssignmentProto& interval_var_assignment_proto) {
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

void IntervalVarElement::RestoreToProto(
    IntervalVarAssignmentProto* interval_var_assignment_proto) const {
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

string IntervalVarElement::DebugString() const {
  if (Activated()) {
    string out;
    SStringPrintf(&out, "(start = %" GG_LL_FORMAT "d", start_min_);
    if (start_max_ != start_min_) {
      StringAppendF(&out, "..%" GG_LL_FORMAT "d", start_max_);
    }
    StringAppendF(&out, ", duration = %" GG_LL_FORMAT "d", duration_min_);
    if (duration_max_ != duration_min_) {
      StringAppendF(&out, "..%" GG_LL_FORMAT "d", duration_max_);
    }
    StringAppendF(&out, ", status = %" GG_LL_FORMAT "d", performed_min_);
    if (performed_max_ != performed_min_) {
      StringAppendF(&out, "..%" GG_LL_FORMAT "d", performed_max_);
    }
    return out;
  } else {
    return "(...)";
  }
}

// ----- Assignment -----

Assignment::Assignment(const Assignment* const copy)
    : PropagationBaseObject(copy->solver()),
      int_var_container_(copy->int_var_container_),
      interval_var_container_(copy->interval_var_container_),
      obj_element_(NULL),
      objective_(NULL) {
  if (copy->obj_element_ != NULL) {
    obj_element_ = copy->obj_element_->Clone();
    objective_ = copy->objective_;
  }
}

Assignment::Assignment(Solver* const s)
  : PropagationBaseObject(s),
    obj_element_(NULL),
    objective_(NULL) {}

Assignment::~Assignment() {
  delete obj_element_;
}

void Assignment::Clear() {
  delete obj_element_;
  obj_element_ = NULL;
  objective_ = NULL;
  int_var_container_.Clear();
  interval_var_container_.Clear();
}

void Assignment::Store() {
  int_var_container_.Store();
  interval_var_container_.Store();
  if (obj_element_ != NULL) {
    obj_element_->Store();
  }
}

void Assignment::Restore() {
  FreezeQueue();
  int_var_container_.Restore();
  interval_var_container_.Restore();
  UnfreezeQueue();
}

namespace {

template <class V, class E>
void IdToElementMap(AssignmentContainer<V, E>* container,
                    hash_map<string, E*>* id_to_element_map) {
  CHECK(id_to_element_map != NULL);
  id_to_element_map->clear();
  for (int i = 0; i < container->Size(); ++i) {
    E& element = container->MutableElement(i);
    const V* const var = element.Var();
    const string& name = var->name();
    if (name.empty()) {
      LOG(INFO) << "Cannot save/load variables with empty name"
                << "; variable will be ignored";
    } else if (ContainsKey(*id_to_element_map, name)) {
      LOG(INFO) << "Cannot save/load variables with duplicate names: " << name
                << "; variable will be ignored";
    } else {
      (*id_to_element_map)[name] = &element;
    }
  }
}

template <class E, class P>
void LoadElement(const hash_map<string, E*>& id_to_element_map,
                 const P& proto) {
  const string& var_id = proto.var_id();
  CHECK(!var_id.empty());
  E* element = NULL;
  if (FindCopy(id_to_element_map, var_id, &element)) {
    element->StoreFromProto(proto);
  } else {
    LOG(INFO) << "Variable " << var_id
              << " not in assignment; skipping variable";
  }
}

}  // namespace

bool Assignment::Load(const string& filename) {
  File::Init();
  File* file = File::Open(filename, "r");
  if (file == NULL) {
    LOG(INFO) << "Cannot open " << filename;
    return false;
  }
  return Load(file);
}

bool Assignment::Load(File* file) {
  CHECK(file != NULL);
  AssignmentProto assignment_proto;
  RecordReader reader(file);
  if (!reader.ReadProtocolMessage(&assignment_proto)) {
    LOG(INFO) << "No assignment found in " << file->CreateFileName();
    return false;
  }
  Load(assignment_proto);
  return reader.Close();
}

void Assignment::Load(const AssignmentProto& assignment_proto) {
  bool fast_load =
      (int_var_container_.Size() == assignment_proto.int_var_assignment_size());
  for (int i = 0;
       fast_load && i < assignment_proto.int_var_assignment_size();
       ++i) {
    IntVarElement& element = int_var_container_.MutableElement(i);
    const IntVarAssignmentProto& proto = assignment_proto.int_var_assignment(i);
    if (element.Var()->name() == proto.var_id()) {
      element.StoreFromProto(proto);
    } else {
      fast_load = false;
    }
  }
  if (!fast_load) {
    hash_map<string, IntVarElement*> id_to_int_var_element_map;
    IdToElementMap<IntVar, IntVarElement>(&int_var_container_,
                                          &id_to_int_var_element_map);
    for (int i = 0; i < assignment_proto.int_var_assignment_size(); ++i) {
      LoadElement<IntVarElement, IntVarAssignmentProto>(
          id_to_int_var_element_map,
          assignment_proto.int_var_assignment(i));
    }
  }
  fast_load =
      (interval_var_container_.Size()
       == assignment_proto.interval_var_assignment_size());
  for (int i = 0;
       fast_load && i < assignment_proto.interval_var_assignment_size();
       ++i) {
    IntervalVarElement& element = interval_var_container_.MutableElement(i);
    const IntervalVarAssignmentProto& proto =
        assignment_proto.interval_var_assignment(i);
    if (element.Var()->name() == proto.var_id()) {
      element.StoreFromProto(proto);
    } else {
      fast_load = false;
    }
  }
  if (!fast_load) {
    hash_map<string, IntervalVarElement*> id_to_interval_var_element_map;
    IdToElementMap<IntervalVar, IntervalVarElement>(
        &interval_var_container_,
        &id_to_interval_var_element_map);
    for (int i = 0; i < assignment_proto.interval_var_assignment_size(); ++i) {
      LoadElement<IntervalVarElement, IntervalVarAssignmentProto>(
          id_to_interval_var_element_map,
          assignment_proto.interval_var_assignment(i));
    }
  }
  if (assignment_proto.has_objective()) {
    const IntVarAssignmentProto& objective = assignment_proto.objective();
    const string objective_id = objective.var_id();
    CHECK(!objective_id.empty());
    if (HasObjective() && objective_id.compare(Objective()->name()) == 0) {
      SetObjectiveRange(objective.min(), objective.max());
      if (objective.active()) {
        ActivateObjective();
      } else {
        DeactivateObjective();
      }
    }
  }
}

bool Assignment::Save(const string& filename) const {
  File::Init();
  File* file = File::Open(filename, "w");
  if (file == NULL) {
    LOG(INFO) << "Cannot open " << filename;
    return false;
  }
  return Save(file);
}

bool Assignment::Save(File* file) const {
  CHECK(file != NULL);
  AssignmentProto assignment_proto;
  Save(&assignment_proto);
  RecordWriter writer(file);
  return writer.WriteProtocolMessage(assignment_proto) && writer.Close();
}

void Assignment::Save(AssignmentProto* const assignment_proto) const {
  assignment_proto->Clear();
  for (int i = 0; i < int_var_container_.Size(); ++i) {
    const IntVarElement& element = int_var_container_.Element(i);
    const IntVar* const var = element.Var();
    const string& name = var->name();
    if (!name.empty()) {
      IntVarAssignmentProto* int_var_assignment_proto =
          assignment_proto->add_int_var_assignment();
      element.RestoreToProto(int_var_assignment_proto);
    }
  }
  for (int i = 0; i < interval_var_container_.Size(); ++i) {
    const IntervalVarElement& element = interval_var_container_.Element(i);
    const IntervalVar* var = element.Var();
    const string& name = var->name();
    if (!name.empty()) {
      IntervalVarAssignmentProto* interval_var_assignment_proto =
          assignment_proto->add_interval_var_assignment();
      element.RestoreToProto(interval_var_assignment_proto);
    }
  }
  if (HasObjective()) {
    const IntVar* objective = Objective();
    const string& name = objective->name();
    if (!name.empty()) {
      IntVarAssignmentProto* objective = assignment_proto->mutable_objective();
      objective->set_var_id(name);
      objective->set_min(ObjectiveMin());
      objective->set_max(ObjectiveMax());
      objective->set_active(ActivatedObjective());
    }
  }
}

string Assignment::DebugString() const {
  string out = "Assignment(";
  for (int i = 0; i < int_var_container_.Size(); ++i) {
    const IntVarElement& element = int_var_container_.Element(i);
    StringAppendF(&out, "%s %s | ",
                  element.Var()->name().c_str(),
                  element.DebugString().c_str());
  }
  for (int i = 0; i < interval_var_container_.Size(); ++i) {
    const IntervalVarElement& element = interval_var_container_.Element(i);
    StringAppendF(&out, "%s %s | ",
                  element.Var()->name().c_str(),
                  element.DebugString().c_str());
  }
  if (obj_element_ != NULL && obj_element_->Activated()) {
    out += obj_element_->DebugString();
  }
  out += ")";
  return out;
}

IntVarElement& Assignment::Add(IntVar* const v) {
  return int_var_container_.Add(v);
}

void Assignment::Add(IntVar* const* v, int s) {
  DCHECK_GE(s, 0);
  for (int i = 0; i < s; ++i) {
    Add(v[i]);
  }
}

void Assignment::Add(const std::vector<IntVar*>& v) {
  for (ConstIter<std::vector<IntVar*> > it(v); !it.at_end(); ++it) {
    Add(*it);
  }
}

IntVarElement& Assignment::FastAdd(IntVar* const v) {
  return int_var_container_.FastAdd(v);
}

int64 Assignment::Min(const IntVar* const v) const {
  return int_var_container_.Element(v).Min();
}

int64 Assignment::Max(const IntVar* const v) const {
  return int_var_container_.Element(v).Max();
}

int64 Assignment::Value(const IntVar* const v) const {
  return int_var_container_.Element(v).Value();
}

bool Assignment::Bound(const IntVar* const v) const {
  return int_var_container_.Element(v).Bound();
}

void Assignment::SetMin(const IntVar* const v, int64 m) {
  int_var_container_.MutableElement(v).SetMin(m);
}

void Assignment::SetMax(const IntVar* const v, int64 m) {
  int_var_container_.MutableElement(v).SetMax(m);
}

void Assignment::SetRange(const IntVar* const v, int64 l, int64 u) {
  int_var_container_.MutableElement(v).SetRange(l, u);
}

void Assignment::SetValue(const IntVar* const v, int64 value) {
  int_var_container_.MutableElement(v).SetValue(value);
}

// ----- Interval Var -----

IntervalVarElement& Assignment::Add(IntervalVar* const v) {
  return interval_var_container_.Add(v);
}

void Assignment::Add(IntervalVar* const * vars, int size) {
  DCHECK_GE(size, 0);
  for (int i = 0; i < size; ++i) {
    Add(vars[i]);
  }
}

void Assignment::Add(const std::vector<IntervalVar*>& vars) {
  for (ConstIter<std::vector<IntervalVar*> > it(vars); !it.at_end(); ++it) {
    Add(*it);
  }
}

IntervalVarElement& Assignment::FastAdd(IntervalVar* const v) {
  return interval_var_container_.FastAdd(v);
}

int64 Assignment::StartMin(const IntervalVar* const v) const {
  return interval_var_container_.Element(v).StartMin();
}

int64 Assignment::StartMax(const IntervalVar* const v) const {
  return interval_var_container_.Element(v).StartMax();
}

int64 Assignment::StartValue(const IntervalVar* const v) const {
  return interval_var_container_.Element(v).StartValue();
}

int64 Assignment::DurationMin(const IntervalVar* const v) const {
  return interval_var_container_.Element(v).DurationMin();
}

int64 Assignment::DurationMax(const IntervalVar* const v) const {
  return interval_var_container_.Element(v).DurationMax();
}

int64 Assignment::DurationValue(const IntervalVar* const v) const {
  return interval_var_container_.Element(v).DurationValue();
}

int64 Assignment::EndMin(const IntervalVar* const v) const {
  return interval_var_container_.Element(v).EndMin();
}

int64 Assignment::EndMax(const IntervalVar* const v) const {
  return interval_var_container_.Element(v).EndMax();
}

int64 Assignment::EndValue(const IntervalVar* const v) const {
  return interval_var_container_.Element(v).EndValue();
}

int64 Assignment::PerformedMin(const IntervalVar* const v) const {
  return interval_var_container_.Element(v).PerformedMin();
}

int64 Assignment::PerformedMax(const IntervalVar* const v) const {
  return interval_var_container_.Element(v).PerformedMax();
}

int64 Assignment::PerformedValue(const IntervalVar* const v) const {
  return interval_var_container_.Element(v).PerformedValue();
}

void Assignment::SetStartMin(const IntervalVar* const v, int64 m) {
  interval_var_container_.MutableElement(v).SetStartMin(m);
}

void Assignment::SetStartMax(const IntervalVar* const v, int64 m) {
  interval_var_container_.MutableElement(v).SetStartMax(m);
}

void Assignment::SetStartRange(const IntervalVar* const v, int64 mi, int64 ma) {
  interval_var_container_.MutableElement(v).SetStartRange(mi, ma);
}

void Assignment::SetStartValue(const IntervalVar* const v, int64 value) {
  interval_var_container_.MutableElement(v).SetStartValue(value);
}

void Assignment::SetDurationMin(const IntervalVar* const v, int64 m) {
  interval_var_container_.MutableElement(v).SetDurationMin(m);
}

void Assignment::SetDurationMax(const IntervalVar* const v, int64 m) {
  interval_var_container_.MutableElement(v).SetDurationMax(m);
}

void Assignment::SetDurationRange(const IntervalVar* const v,
                                  int64 mi, int64 ma) {
  interval_var_container_.MutableElement(v).SetDurationRange(mi, ma);
}

void Assignment::SetDurationValue(const IntervalVar* const v, int64 value) {
  interval_var_container_.MutableElement(v).SetDurationValue(value);
}

void Assignment::SetEndMin(const IntervalVar* const v, int64 m) {
  interval_var_container_.MutableElement(v).SetEndMin(m);
}

void Assignment::SetEndMax(const IntervalVar* const v, int64 m) {
  interval_var_container_.MutableElement(v).SetEndMax(m);
}

void Assignment::SetEndRange(const IntervalVar* const v, int64 mi, int64 ma) {
  interval_var_container_.MutableElement(v).SetEndRange(mi, ma);
}

void Assignment::SetEndValue(const IntervalVar* const v, int64 value) {
  interval_var_container_.MutableElement(v).SetEndValue(value);
}

void Assignment::SetPerformedMin(const IntervalVar* const v, int64 m) {
  interval_var_container_.MutableElement(v).SetPerformedMin(m);
}

void Assignment::SetPerformedMax(const IntervalVar* const v, int64 m) {
  interval_var_container_.MutableElement(v).SetPerformedMax(m);
}

void Assignment::SetPerformedRange(const IntervalVar* const v,
                                   int64 mi, int64 ma) {
  interval_var_container_.MutableElement(v).SetPerformedRange(mi, ma);
}

void Assignment::SetPerformedValue(const IntervalVar* const v, int64 value) {
  interval_var_container_.MutableElement(v).SetPerformedValue(value);
}

void Assignment::AddObjective(IntVar* const v) {
  // Check if adding twice an objective to the solution.
  CHECK(obj_element_ == NULL);
  obj_element_ = new IntVarElement(v);
  objective_ = v;
}

IntVar* Assignment::Objective() const {
  return objective_;
}

bool Assignment::HasObjective() const {
  return (obj_element_ != NULL);
}

int64 Assignment::ObjectiveMin() const {
  if (obj_element_ != NULL) {
    return obj_element_->Min();
  }
  return 0;
}

int64 Assignment::ObjectiveMax() const {
  if (obj_element_ != NULL) {
    return obj_element_->Max();
  }
  return 0;
}

int64 Assignment::ObjectiveValue() const {
  if (obj_element_ != NULL) {
    return obj_element_->Value();
  }
  return 0;
}

bool Assignment::ObjectiveBound() const {
  if (obj_element_ != NULL) {
    return obj_element_->Bound();
  }
  return true;
}

void Assignment::SetObjectiveMin(int64 m) {
  if (obj_element_ != NULL) {
    obj_element_->SetMin(m);
  }
}

void Assignment::SetObjectiveMax(int64 m) {
  if (obj_element_ != NULL) {
    obj_element_->SetMax(m);
  }
}

void Assignment::SetObjectiveRange(int64 l, int64 u) {
  if (obj_element_ != NULL) {
    obj_element_->SetRange(l, u);
  }
}

void Assignment::SetObjectiveValue(int64 value) {
  if (obj_element_ != NULL) {
    obj_element_->SetValue(value);
  }
}

void Assignment::Activate(const IntVar* const b) {
  int_var_container_.MutableElement(b).Activate();
}

void Assignment::Deactivate(const IntVar* const b) {
  int_var_container_.MutableElement(b).Deactivate();
}

bool Assignment::Activated(const IntVar* const b) const {
  return int_var_container_.Element(b).Activated();
}

void Assignment::Activate(const IntervalVar* const b) {
  interval_var_container_.MutableElement(b).Activate();
}

void Assignment::Deactivate(const IntervalVar* const b) {
  interval_var_container_.MutableElement(b).Deactivate();
}

bool Assignment::Activated(const IntervalVar* const b) const {
  return interval_var_container_.Element(b).Activated();
}

void Assignment::ActivateObjective() {
  if (obj_element_ != NULL) {
    obj_element_->Activate();
  }
}

void Assignment::DeactivateObjective() {
  if (obj_element_ != NULL) {
    obj_element_->Deactivate();
  }
}

bool Assignment::ActivatedObjective() const {
  if (obj_element_ != NULL) {
    return obj_element_->Activated();
  }
  return true;
}

bool Assignment::Contains(const IntVar* const var) const {
  return int_var_container_.Contains(var);
}

bool Assignment::Contains(const IntervalVar* const var) const {
  return interval_var_container_.Contains(var);
}

void Assignment::Copy(const Assignment* assignment) {
  int_var_container_.Copy(assignment->int_var_container_);
  interval_var_container_.Copy(assignment->interval_var_container_);
  if (assignment->HasObjective() && HasObjective()) {
    IntVarElement* obj_element = assignment->obj_element_;
    obj_element_->SetRange(obj_element->Min(), obj_element->Max());
    if (obj_element->Activated())
      obj_element_->Activate();
    else
      obj_element_->Deactivate();
  }
}

Assignment* Solver::MakeAssignment() {
  return RevAlloc(new Assignment(this));
}

Assignment* Solver::MakeAssignment(const Assignment* const a) {
  return RevAlloc(new Assignment(a));
}

// ----- Restoring assignments -----
namespace {
class RestoreAssignment : public DecisionBuilder {
 public:
  explicit RestoreAssignment(Assignment* assignment);
  virtual ~RestoreAssignment() {}
  virtual Decision* Next(Solver* const solver);
  virtual string DebugString() const {
    return "RestoreAssignment";
  }
 private:
  Assignment* const assignment_;
};

RestoreAssignment::RestoreAssignment(Assignment* assignment)
    : assignment_(assignment) {}

Decision* RestoreAssignment::Next(Solver* /*solver*/) {
  assignment_->Restore();
  return NULL;
}
}  // namespace

DecisionBuilder* Solver::MakeRestoreAssignment(Assignment* assignment) {
  return RevAlloc(new RestoreAssignment(assignment));
}

// ----- Storing assignments -----
namespace {
class StoreAssignment : public DecisionBuilder {
 public:
  explicit StoreAssignment(Assignment* assignment);
  virtual ~StoreAssignment() {}
  virtual Decision* Next(Solver* const solver);
  virtual string DebugString() const {
    return "StoreAssignment";
  }
 private:
  Assignment* const assignment_;
};

StoreAssignment::StoreAssignment(Assignment* assignment)
    : assignment_(assignment) {}

Decision* StoreAssignment::Next(Solver* /*solver*/) {
  assignment_->Store();
  return NULL;
}
}  // namespace

DecisionBuilder* Solver::MakeStoreAssignment(Assignment* assignment) {
  return RevAlloc(new StoreAssignment(assignment));
}

}  // namespace operations_research
