// Copyright 2010 Google
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

#include "ortools/base/stl_util.h"
#include "ortools/constraint_solver/constraint_solveri.h"
#include "examples/cpp/global_arith.h"

namespace operations_research {

class ArithmeticPropagator;

// ----- SubstitutionMap -----

class SubstitutionMap {
 public:
  struct Offset {  // to_replace = var_index + offset
    Offset() : var_index(-1), offset(0) {}
    Offset(int v, int64 o) : var_index(v), offset(o) {}
    int var_index;
    int64 offset;
  };

  void AddSubstitution(int left_var, int right_var, int64 right_offset) {
    // TODO(user) : Perform transitive closure.
    substitutions_[left_var] = Offset(right_var, right_offset);
  }

  void ProcessAllSubstitutions(Callback3<int, int, int64>* const hook) {
    for (std::unordered_map<int, Offset>::const_iterator it = substitutions_.begin();
         it != substitutions_.end();
         ++it) {
      hook->Run(it->first, it->second.var_index, it->second.offset);
    }
  }
 private:
  std::unordered_map<int, Offset> substitutions_;
};

// ----- Bounds -----

struct Bounds {
  Bounds() : lb(kint64min), ub(kint64max) {}
  Bounds(int64 l, int64 u) : lb(l), ub(u) {}

  void Intersect(int64 new_lb, int64 new_ub) {
    lb = std::max(lb, new_lb);
    ub = std::min(ub, new_ub);
  }

  void Intersect(const Bounds& other) {
    Intersect(other.lb, other.ub);
  }

  void Union(int64 new_lb, int64 new_ub) {
    lb = std::min(lb, new_lb);
    ub = std::max(ub, new_ub);
  }

  void Union(const Bounds& other) {
    Union(other.lb, other.ub);
  }

  bool IsEqual(const Bounds& other) {
    return (ub == other.ub && lb == other.lb);
  }

  bool IsIncluded(const Bounds& other) {
    return (ub <= other.ub && lb >= other.lb);
  }

  int64 lb;
  int64 ub;
};

// ----- BoundsStore -----

class BoundsStore {
 public:
  BoundsStore(vector<Bounds>* initial_bounds)
      : initial_bounds_(initial_bounds) {}

  void SetRange(int var_index, int64 lb, int64 ub) {
    std::unordered_map<int, Bounds>::iterator it = modified_bounds_.find(var_index);
    if (it == modified_bounds_.end()) {
      Bounds new_bounds(lb, ub);
      const Bounds& initial = (*initial_bounds_)[var_index];
      new_bounds.Intersect(initial);
      if (!new_bounds.IsEqual(initial)) {
        modified_bounds_.insert(make_pair(var_index, new_bounds));
      }
    } else {
      it->second.Intersect(lb, ub);
    }
  }

  void Clear() {
    modified_bounds_.clear();
  }

  const std::unordered_map<int, Bounds>& modified_bounds() const {
    return modified_bounds_;
  }

  vector<Bounds>* initial_bounds() const { return initial_bounds_; }

  void Apply() {
    for (std::unordered_map<int, Bounds>::const_iterator it = modified_bounds_.begin();
         it != modified_bounds_.end();
         ++it) {
      (*initial_bounds_)[it->first] = it->second;
    }
  }

 private:
  vector<Bounds>* initial_bounds_;
  std::unordered_map<int, Bounds> modified_bounds_;
};

// ----- ArithmeticConstraint -----

class ArithmeticConstraint {
 public:
  virtual ~ArithmeticConstraint() {}

  const vector<int>& vars() const { return vars_; }

  virtual bool Propagate(BoundsStore* const store) = 0;
  virtual void Replace(int to_replace, int var, int64 offset) = 0;
  virtual bool Deduce(ArithmeticPropagator* const propagator) const = 0;
  virtual string DebugString() const = 0;
 private:
  const vector<int> vars_;
};

// ----- ArithmeticPropagator -----

class ArithmeticPropagator : PropagationBaseObject {
 public:
  ArithmeticPropagator(Solver* const solver, Demon* const demon)
      : PropagationBaseObject(solver), demon_(demon) {}

  void ReduceProblem() {
    for (int constraint_index = 0;
         constraint_index < constraints_.size();
         ++constraint_index) {
      if (constraints_[constraint_index]->Deduce(this)) {
        protected_constraints_.insert(constraint_index);
      }
    }
    scoped_ptr<Callback3<int, int, int64> > hook(
        NewPermanentCallback(this,
                             &ArithmeticPropagator::ProcessOneSubstitution));
    substitution_map_.ProcessAllSubstitutions(hook.get());
  }

  void Post() {
    for (int constraint_index = 0;
         constraint_index < constraints_.size();
         ++constraint_index) {
      const vector<int>& vars = constraints_[constraint_index]->vars();
      for (int var_index = 0; var_index < vars.size(); ++var_index) {
        dependencies_[vars[var_index]].push_back(constraint_index);
      }
    }
  }

  void InitialPropagate() {

  }

  void Update(int var_index) {
    Enqueue(demon_);
  }

  void AddConstraint(ArithmeticConstraint* const ct) {
    constraints_.push_back(ct);
  }

  void AddVariable(int64 lb, int64 ub) {
    bounds_.push_back(Bounds(lb, ub));
  }

  const vector<IntVar*> vars() const { return vars_; }

  int VarIndex(IntVar* const var) {
    std::unordered_map<IntVar*, int>::const_iterator it = var_map_.find(var);
    if (it == var_map_.end()) {
      const int index = var_map_.size();
      var_map_[var] = index;
      return index;
    } else {
      return it->second;
    }
  }

  void AddSubstitution(int left_var, int right_var, int64 right_offset) {
    substitution_map_.AddSubstitution(left_var, right_var, right_offset);
  }

  void AddNewBounds(int var_index, int64 lb, int64 ub) {
    bounds_[var_index].Intersect(lb, ub);
  }

  void ProcessOneSubstitution(int left_var, int right_var, int64 right_offset) {
    for (int constraint_index = 0;
         constraint_index < constraints_.size();
         ++constraint_index) {
      if (!ContainsKey(protected_constraints_, constraint_index)) {
        ArithmeticConstraint* const constraint = constraints_[constraint_index];
        constraint->Replace(left_var, right_var, right_offset);
      }
    }
  }

  void PrintModel() {
    LOG(INFO) << "Vars:";
    for (int i = 0; i < bounds_.size(); ++i) {
      LOG(INFO) << "  var<" << i << "> = [" << bounds_[i].lb
                << " .. " << bounds_[i].ub << "]";
    }
    LOG(INFO) << "Constraints";
    for (int i = 0; i < constraints_.size(); ++i) {
      LOG(INFO) << "  " << constraints_[i]->DebugString();
    }
  }
 private:
  Demon* const demon_;
  vector<IntVar*> vars_;
  std::unordered_map<IntVar*, int> var_map_;
  vector<ArithmeticConstraint*> constraints_;
  vector<Bounds> bounds_;
  vector<vector<int> > dependencies_;  // from var indices to constraints.
  SubstitutionMap substitution_map_;
  std::unordered_set<int> protected_constraints_;
};

// ----- Custom Constraints -----

class RowConstraint : public ArithmeticConstraint {
 public:
  RowConstraint(int64 lb, int64 ub) : lb_(lb), ub_(ub) {}
  virtual ~RowConstraint() {}

  void AddTerm(int var_index, int64 coefficient) {
    // TODO(user): Check not present.
    coefficients_[var_index] = coefficient;
  }

  virtual bool Propagate(BoundsStore* const store) {
    return true;
  }

  virtual void Replace(int to_replace, int var, int64 offset) {
    std::unordered_map<int, int64>::iterator find_other = coefficients_.find(to_replace);
    if (find_other != coefficients_.end()) {
      std::unordered_map<int, int64>::iterator find_var = coefficients_.find(var);
      const int64 other_coefficient = find_other->second;
      if (lb_ != kint64min) {
        lb_ += other_coefficient * offset;
      }
      if (ub_ != kint64max) {
        ub_ += other_coefficient * offset;
      }
      coefficients_.erase(find_other);
      if (find_var == coefficients_.end()) {
        coefficients_[var] = other_coefficient;
      } else {
        find_var->second += other_coefficient;
        if (find_var->second == 0) {
          coefficients_.erase(find_var);
        }
      }
    }
  }

  virtual bool Deduce(ArithmeticPropagator* const propagator) const {
    // Deduce Simple translation from one var to another.
    if (lb_ == ub_ && coefficients_.size() == 2) {
      std::unordered_map<int, int64>::const_iterator it = coefficients_.begin();
      const int var1 = it->first;
      const int64 coeff1 = it->second;
      ++it;
      const int var2 = it->first;
      const int64 coeff2 = it->second;
      ++it;
      CHECK(it == coefficients_.end());
      if (coeff1 == 1 && coeff2 == -1) {
        propagator->AddSubstitution(var1, var2, lb_);
        return true;
      } else if (coeff1 == -1 && coeff2 && 1) {
        propagator->AddSubstitution(var2, var1, lb_);
        return true;
      }
    }
    return false;
  }

  virtual string DebugString() const {
    string output = "(";
    bool first = true;
    for (std::unordered_map<int, int64>::const_iterator it = coefficients_.begin();
         it != coefficients_.end();
         ++it) {
      if (it->second != 0) {
        if (first) {
          first = false;
          if (it->second == 1) {
            output += StringPrintf("var<%d>", it->first);
          } else if (it->second == -1) {
          output += StringPrintf("-var<%d>", it->first);
          } else {
            output += StringPrintf("%lld*var<%d>", it->second, it->first);
          }
        } else if (it->second == 1) {
          output += StringPrintf(" + var<%d>", it->first);
        } else if (it->second == -1) {
          output += StringPrintf(" - var<%d>", it->first);
        } else if (it->second > 0) {
          output += StringPrintf(" + %lld*var<%d>", it->second, it->first);
        } else {
          output += StringPrintf(" - %lld*var<%d>", -it->second, it->first);
        }
      }
    }
    if (lb_ == ub_) {
      output += StringPrintf(" == %lld)", ub_);
    } else if (lb_ == kint64min) {
      output += StringPrintf(" <= %lld)", ub_);
    } else if (ub_ == kint64max) {
      output += StringPrintf(" >= %lld)", lb_);
    } else {
      output += StringPrintf(" in [%lld .. %lld])", lb_, ub_);
    }
    return output;
  }
 private:
  std::unordered_map<int, int64> coefficients_;
  int64 lb_;
  int64 ub_;
};

class OrConstraint : public ArithmeticConstraint {
 public:
  OrConstraint(ArithmeticConstraint* const left,
               ArithmeticConstraint* const right)
      : left_(left), right_(right) {}

  virtual ~OrConstraint() {}

  virtual bool Propagate(BoundsStore* const store) {
    return true;
  }

  virtual void Replace(int to_replace, int var, int64 offset) {
    left_->Replace(to_replace, var, offset);
    right_->Replace(to_replace, var, offset);
  }

  virtual bool Deduce(ArithmeticPropagator* const propagator) const {
    return false;
  }

  virtual string DebugString() const {
    return StringPrintf("Or(%s, %s)",
                        left_->DebugString().c_str(),
                        right_->DebugString().c_str());
  }
 private:
  ArithmeticConstraint* const left_;
  ArithmeticConstraint* const right_;
};

// ----- GlobalArithmeticConstraint -----

GlobalArithmeticConstraint::GlobalArithmeticConstraint(Solver* const solver)
    : Constraint(solver),
      propagator_(NULL) {
  propagator_.reset(new ArithmeticPropagator(
      solver,
      solver->MakeDelayedConstraintInitialPropagateCallback(this)));
}
GlobalArithmeticConstraint::~GlobalArithmeticConstraint() {
  STLDeleteElements(&constraints_);
}

void GlobalArithmeticConstraint::Post() {
  const vector<IntVar*>& vars = propagator_->vars();
  for (int var_index = 0; var_index < vars.size(); ++var_index) {
    Demon* const demon =
        MakeConstraintDemon1(solver(),
                             this,
                             &GlobalArithmeticConstraint::Update,
                             "Update",
                             var_index);
    vars[var_index]->WhenRange(demon);
  }
  LOG(INFO) << "----- Before reduction -----";
  propagator_->PrintModel();
  LOG(INFO) << "----- After reduction -----";
  propagator_->ReduceProblem();
  propagator_->PrintModel();
  LOG(INFO) << "---------------------------";
  propagator_->Post();
}

void GlobalArithmeticConstraint::InitialPropagate() {
  propagator_->InitialPropagate();
}

void GlobalArithmeticConstraint::Update(int var_index) {
  propagator_->Update(var_index);
}

ConstraintRef GlobalArithmeticConstraint::MakeScalProdGreaterOrEqualConstant(
    const vector<IntVar*> vars,
    const vector<int64> coefficients,
    int64 constant) {
  RowConstraint* const constraint = new RowConstraint(constant, kint64max);
  for (int index = 0; index < vars.size(); ++index) {
    constraint->AddTerm(VarIndex(vars[index]), coefficients[index]);
  }
  return Store(constraint);
}

ConstraintRef GlobalArithmeticConstraint::MakeScalProdLessOrEqualConstant(
    const vector<IntVar*> vars,
    const vector<int64> coefficients,
    int64 constant) {
  RowConstraint* const constraint = new RowConstraint(kint64min, constant);
  for (int index = 0; index < vars.size(); ++index) {
    constraint->AddTerm(VarIndex(vars[index]), coefficients[index]);
  }
  return Store(constraint);
}

ConstraintRef GlobalArithmeticConstraint::MakeScalProdEqualConstant(
    const vector<IntVar*> vars,
    const vector<int64> coefficients,
    int64 constant) {
  RowConstraint* const constraint = new RowConstraint(constant, constant);
  for (int index = 0; index < vars.size(); ++index) {
    constraint->AddTerm(VarIndex(vars[index]), coefficients[index]);
  }
  return Store(constraint);
}

ConstraintRef GlobalArithmeticConstraint::MakeSumGreaterOrEqualConstant(
    const vector<IntVar*> vars,
    int64 constant) {
  RowConstraint* const constraint = new RowConstraint(constant, kint64max);
  for (int index = 0; index < vars.size(); ++index) {
    constraint->AddTerm(VarIndex(vars[index]), 1);
  }
  return Store(constraint);
}

ConstraintRef GlobalArithmeticConstraint::MakeSumLessOrEqualConstant(
    const vector<IntVar*> vars, int64 constant) {
  RowConstraint* const constraint = new RowConstraint(kint64min, constant);
  for (int index = 0; index < vars.size(); ++index) {
    constraint->AddTerm(VarIndex(vars[index]), 1);
  }
  return Store(constraint);
}

ConstraintRef GlobalArithmeticConstraint::MakeSumEqualConstant(
    const vector<IntVar*> vars, int64 constant) {
  RowConstraint* const constraint = new RowConstraint(constant, constant);
  for (int index = 0; index < vars.size(); ++index) {
    constraint->AddTerm(VarIndex(vars[index]), 1);
  }
  return Store(constraint);
}

ConstraintRef GlobalArithmeticConstraint::MakeRowConstraint(
    int64 lb,
    const vector<IntVar*> vars,
    const vector<int64> coefficients,
    int64 ub) {
  RowConstraint* const constraint = new RowConstraint(lb, ub);
  for (int index = 0; index < vars.size(); ++index) {
    constraint->AddTerm(VarIndex(vars[index]), coefficients[index]);
  }
  return Store(constraint);
}

ConstraintRef GlobalArithmeticConstraint::MakeRowConstraint(int64 lb,
                                                  IntVar* const v1,
                                                  int64 coeff1,
                                                  int64 ub) {
  RowConstraint* const constraint = new RowConstraint(lb, ub);
  constraint->AddTerm(VarIndex(v1), coeff1);
  return Store(constraint);
}

ConstraintRef GlobalArithmeticConstraint::MakeRowConstraint(int64 lb,
                                                            IntVar* const v1,
                                                            int64 coeff1,
                                                            IntVar* const v2,
                                                            int64 coeff2,
                                                            int64 ub) {
  RowConstraint* const constraint = new RowConstraint(lb, ub);
    constraint->AddTerm(VarIndex(v1), coeff1);
    constraint->AddTerm(VarIndex(v2), coeff2);
    return Store(constraint);
}

ConstraintRef GlobalArithmeticConstraint::MakeRowConstraint(int64 lb,
                                                            IntVar* const v1,
                                                            int64 coeff1,
                                                            IntVar* const v2,
                                                            int64 coeff2,
                                                            IntVar* const v3,
                                                            int64 coeff3,
                                                            int64 ub) {
  RowConstraint* const constraint = new RowConstraint(lb, ub);
  constraint->AddTerm(VarIndex(v1), coeff1);
  constraint->AddTerm(VarIndex(v2), coeff2);
  constraint->AddTerm(VarIndex(v3), coeff3);
  return Store(constraint);
}

ConstraintRef GlobalArithmeticConstraint::MakeRowConstraint(int64 lb,
                                                            IntVar* const v1,
                                                            int64 coeff1,
                                                            IntVar* const v2,
                                                            int64 coeff2,
                                                            IntVar* const v3,
                                                            int64 coeff3,
                                                            IntVar* const v4,
                                                            int64 coeff4,
                                                            int64 ub) {
  RowConstraint* const constraint = new RowConstraint(lb, ub);
  constraint->AddTerm(VarIndex(v1), coeff1);
  constraint->AddTerm(VarIndex(v2), coeff2);
  constraint->AddTerm(VarIndex(v3), coeff3);
  constraint->AddTerm(VarIndex(v4), coeff4);
  return Store(constraint);
}

ConstraintRef GlobalArithmeticConstraint::MakeOrConstraint(
    ConstraintRef left_ref,
    ConstraintRef right_ref) {
  OrConstraint* const constraint =
      new OrConstraint(constraints_[left_ref.index()],
                       constraints_[right_ref.index()]);
  return Store(constraint);
}

void GlobalArithmeticConstraint::Add(ConstraintRef ref) {
  propagator_->AddConstraint(constraints_[ref.index()]);
}

int GlobalArithmeticConstraint::VarIndex(IntVar* const var) {
  std::unordered_map<IntVar*, int>::const_iterator it = var_indices_.find(var);
  if (it == var_indices_.end()) {
    const int new_index = var_indices_.size();
    var_indices_.insert(make_pair(var, new_index));
    propagator_->AddVariable(var->Min(), var->Max());
    return new_index;
  } else {
    return it->second;
  }
}

ConstraintRef GlobalArithmeticConstraint::Store(
    ArithmeticConstraint* const constraint) {
  const int constraint_index = constraints_.size();
  constraints_.push_back(constraint);
  return ConstraintRef(constraint_index);
}
}  // namespace operations_research
