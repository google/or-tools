// Copyright 2010 Google Inc. All Rights Reserved.
// Author: lperron@google.com (Laurent Perron)

#include "base/commandlineflags.h"
#include "base/integral_types.h"
#include "base/logging.h"
#include "base/scoped_ptr.h"
#include "base/stl_util-inl.h"
#include "base/stringprintf.h"
#include "constraint_solver/constraint_solveri.h"

DEFINE_int32(size, 20, "Size of the problem");

namespace operations_research {

// ---------- Arithmetic Propagator ----------

namespace {

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
    // TODO(lperron) : Perform transitive closure.
    substitutions_[left_var] = Offset(right_var, right_offset);
  }

  void ProcessAllSubstitutions(Callback3<int, int, int64>* const hook) {
    for (hash_map<int, Offset>::const_iterator it = substitutions_.begin();
         it != substitutions_.end();
         ++it) {
      hook->Run(it->first, it->second.var_index, it->second.offset);
    }
  }
 private:
  hash_map<int, Offset> substitutions_;
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
    hash_map<int, Bounds>::iterator it = modified_bounds_.find(var_index);
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

  const hash_map<int, Bounds>& modified_bounds() const {
    return modified_bounds_;
  }

  vector<Bounds>* initial_bounds() const { return initial_bounds_; }

  void Apply() {
    for (hash_map<int, Bounds>::const_iterator it = modified_bounds_.begin();
         it != modified_bounds_.end();
         ++it) {
      (*initial_bounds_)[it->first] = it->second;
    }
  }
    
 private:
  vector<Bounds>* initial_bounds_;
  hash_map<int, Bounds> modified_bounds_;
};

// ----- ArithmeticConstraint -----

class ArithmeticConstraint {
 public:
  virtual ~ArithmeticConstraint() {}

  const vector<int>& vars() const { return vars_; }

  virtual bool Propagate(BoundsStore* const store) = 0;
  virtual void Replace(int to_replace, int var, int64 offset) = 0;
  virtual void Deduce(ArithmeticPropagator* const propagator) const = 0;
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
      constraints_[constraint_index]->Deduce(this);
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
    hash_map<IntVar*, int>::const_iterator it = var_map_.find(var);
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
      ArithmeticConstraint* const constraint = constraints_[constraint_index];
      constraint->Replace(left_var, right_var, right_offset);
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
  hash_map<IntVar*, int> var_map_;
  vector<ArithmeticConstraint*> constraints_;
  vector<Bounds> bounds_;
  vector<vector<int> > dependencies_;  // from var indices to constraints.
  SubstitutionMap substitution_map_;
};

// ----- Custom Constraints -----

class VarEqualVarPlusOffset : public ArithmeticConstraint {
 public:
  VarEqualVarPlusOffset(int left_var, int right_var, int64 right_offset)
      : left_var_(left_var), right_var_(right_var),
        right_offset_(right_offset) {}

  virtual bool Propagate(BoundsStore* const store) {
    return true;
  }

  virtual void Replace(int to_replace, int var, int64 offset) {
    if ((to_replace == left_var_ &&
         var == right_offset_ &&
         offset == right_offset_) ||
        (to_replace == right_var_ &&
         var == left_var_ && offset == -right_offset_)) {
      return;
    }
    if (to_replace == left_var_) {
      left_var_ = to_replace;
      right_offset_ -= offset;
      return;
    }
    if (to_replace == right_var_) {
      right_var_ = var;
      right_offset_ += offset;
      return;
    }
  }

  virtual void Deduce(ArithmeticPropagator* const propagator) const {
    propagator->AddSubstitution(left_var_, right_var_, right_offset_);
  }

  virtual string DebugString() const {
    if (right_offset_ == 0) {
      return StringPrintf("var<%d> == var<%d>", left_var_, right_var_);
    } else {
      return StringPrintf("var<%d> == var<%d> + %lld",
                          left_var_,
                          right_var_,
                          right_offset_);
    }
  }
 private:
  int left_var_;
  int right_var_;
  int64 right_offset_;
};

class RowConstraint : public ArithmeticConstraint {
 public:
  RowConstraint(int64 lb, int64 ub) : lb_(lb), ub_(ub) {}
  virtual ~RowConstraint() {}

  void AddTerm(int var_index, int64 coefficient) {
    // TODO(lperron): Check not present.
    coefficients_[var_index] = coefficient;
  }

  virtual bool Propagate(BoundsStore* const store) {
    return true;
  }

  virtual void Replace(int to_replace, int var, int64 offset) {
    hash_map<int, int64>::iterator find_other = coefficients_.find(to_replace);
    if (find_other != coefficients_.end()) {
      hash_map<int, int64>::iterator find_var = coefficients_.find(var);
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

  virtual void Deduce(ArithmeticPropagator* const propagator) const {}

  virtual string DebugString() const {
    string output = "(";
    bool first = true;
    for (hash_map<int, int64>::const_iterator it = coefficients_.begin();
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
  hash_map<int, int64> coefficients_;
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

  virtual void Deduce(ArithmeticPropagator* const propagator) const {}

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

class GlobalArithmeticConstraint : public Constraint {
 public:
  GlobalArithmeticConstraint(Solver* const solver)
      : Constraint(solver),
        propagator_(
            solver,
            solver->MakeDelayedConstraintInitialPropagateCallback(this)) {}
  virtual ~GlobalArithmeticConstraint() {
    STLDeleteElements(&constraints_);
  }

  virtual void Post() {
    const vector<IntVar*>& vars = propagator_.vars();
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
    propagator_.PrintModel();
    LOG(INFO) << "----- After reduction -----";
    propagator_.ReduceProblem();
    propagator_.PrintModel();
    LOG(INFO) << "---------------------------";
    propagator_.Post();
  }

  virtual void InitialPropagate() {
    propagator_.InitialPropagate();
  }

  void Update(int var_index) {
    propagator_.Update(var_index);
  }

  int MakeVarEqualVarPlusOffset(IntVar* const left_var, 
                                IntVar* const right_var,
                                int64 right_offset) {
    const int left_index = VarIndex(left_var);
    const int right_index = VarIndex(right_var);
    return Store(new VarEqualVarPlusOffset(left_index,
                                           right_index,
                                           right_offset));
  }

  int MakeScalProdGreaterOrEqualConstant(const vector<IntVar*> vars,
                                         const vector<int64> coefficients, 
                                         int64 constant) {
    RowConstraint* const constraint = new RowConstraint(constant, kint64max);
    for (int index = 0; index < vars.size(); ++index) {
      constraint->AddTerm(VarIndex(vars[index]), coefficients[index]);
    }
    return Store(constraint);
  }

  int MakeScalProdLessOrEqualConstant(const vector<IntVar*> vars,
                                      const vector<int64> coefficients, 
                                      int64 constant) {
    RowConstraint* const constraint = new RowConstraint(kint64min, constant);
    for (int index = 0; index < vars.size(); ++index) {
      constraint->AddTerm(VarIndex(vars[index]), coefficients[index]);
    }
    return Store(constraint);
  }

  int MakeScalProdEqualConstant(const vector<IntVar*> vars,
                                const vector<int64> coefficients, 
                                int64 constant) {
    RowConstraint* const constraint = new RowConstraint(constant, constant);    
    for (int index = 0; index < vars.size(); ++index) {
      constraint->AddTerm(VarIndex(vars[index]), coefficients[index]);
    }
    return Store(constraint);
  }

  int MakeSumGreaterOrEqualConstant(const vector<IntVar*> vars,
                                    int64 constant) {
    RowConstraint* const constraint = new RowConstraint(constant, kint64max);
    for (int index = 0; index < vars.size(); ++index) {
      constraint->AddTerm(VarIndex(vars[index]), 1);
    }
    return Store(constraint);
  }

  int MakeSumLessOrEqualConstant(const vector<IntVar*> vars, int64 constant) {
    RowConstraint* const constraint = new RowConstraint(kint64min, constant);
    for (int index = 0; index < vars.size(); ++index) {
      constraint->AddTerm(VarIndex(vars[index]), 1);
    }
    return Store(constraint);
  }

  int MakeSumEqualConstant(const vector<IntVar*> vars, int64 constant) {
    RowConstraint* const constraint = new RowConstraint(constant, constant);
    for (int index = 0; index < vars.size(); ++index) {
      constraint->AddTerm(VarIndex(vars[index]), 1);
    }
    return Store(constraint);
  }

  int MakeRowConstraint(int64 lb, 
                        const vector<IntVar*> vars, 
                        const vector<int64> coefficients, 
                        int64 ub) {
    RowConstraint* const constraint = new RowConstraint(lb, ub);    
    for (int index = 0; index < vars.size(); ++index) {
      constraint->AddTerm(VarIndex(vars[index]), coefficients[index]);
    }
    return Store(constraint);
  }

  int MakeRowConstraint(int64 lb, 
                        IntVar* const v1, 
                        int64 coeff1,
                        int64 ub) {
    RowConstraint* const constraint = new RowConstraint(lb, ub);    
    constraint->AddTerm(VarIndex(v1), coeff1);
    return Store(constraint);
  }

  int MakeRowConstraint(int64 lb, 
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

  int MakeRowConstraint(int64 lb, 
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

  int MakeRowConstraint(int64 lb, 
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

  int MakeOrConstraint(int left_constraint_index, int right_constraint_index) {
    OrConstraint* const constraint =
        new OrConstraint(constraints_[left_constraint_index],
                         constraints_[right_constraint_index]);
    return Store(constraint);
  }

  void Add(int constraint_index) {
    propagator_.AddConstraint(constraints_[constraint_index]);
  }
 private:
  int VarIndex(IntVar* const var) {
    hash_map<IntVar*, int>::const_iterator it = var_indices_.find(var);
    if (it == var_indices_.end()) {
      const int new_index = var_indices_.size();
      var_indices_.insert(make_pair(var, new_index));
      propagator_.AddVariable(var->Min(), var->Max());
      return new_index;
    } else {
      return it->second;
    }
  }

  int Store(ArithmeticConstraint* const constraint) {
    const int constraint_index = constraints_.size();
    constraints_.push_back(constraint);
    return constraint_index;
  }

  ArithmeticPropagator propagator_;
  hash_map<IntVar*, int> var_indices_;
  vector<ArithmeticConstraint*> constraints_;
};

}  // namespace

// ---------- Examples ----------

void DeepSearchTreeArith(int size) {
  LOG(INFO) << "DeepSearchTreeArith: size = " << size;
  const int64 rmax = 1 << size;

  Solver solver("DeepSearchTreeArith");
  IntVar* const v1 = solver.MakeIntVar(1, rmax, "v1");
  IntVar* const v2 = solver.MakeIntVar(0, rmax, "v2");
  IntVar* const v3 = solver.MakeIntVar(0, rmax, "v3");

  GlobalArithmeticConstraint* const global =
      solver.RevAlloc(new GlobalArithmeticConstraint(&solver));
  

  global->Add(global->MakeVarEqualVarPlusOffset(v1, v2, 0));
  global->Add(global->MakeVarEqualVarPlusOffset(v2, v3, 0));
  const int left = 
      global->MakeRowConstraint(0, v1, -1, v2, -1, v3, 1, kint64max);
  const int right = 
      global->MakeRowConstraint(0, v1, -1, v2, 1, v3, -1, kint64max);
  global->Add(global->MakeOrConstraint(left, right));

  global->Post();
}

void SlowPropagationArith(int size) {
  LOG(INFO) << "SlowPropagationArith: size = " << size;
  const int64 rmin = -(1 << size);
  const int64 rmax = 1 << size;

  Solver solver("SlowPropagationArith");
  IntVar* const v1 = solver.MakeIntVar(rmin, rmax, "v1");
  IntVar* const v2 = solver.MakeIntVar(rmin, rmax, "v2");

  GlobalArithmeticConstraint* const global =
      solver.RevAlloc(new GlobalArithmeticConstraint(&solver));
  
  global->Add(global->MakeRowConstraint(1, v1, 1, v2, -1, kint64max));
  global->Add(global->MakeRowConstraint(0, v1, -1, v2, 1, kint64max));

  global->Post();
}

void DeepSearchTree(int size){
  LOG(INFO) << "DeepSearchTree: size = " << size;
  Solver s("DeepSearchTree");
  const int64 rmax = 1 << size;
  IntVar* const i = s.MakeIntVar(1, rmax, "i");
  IntVar* const j = s.MakeIntVar(0, rmax, "j");
  IntVar* const k = s.MakeIntVar(0, rmax, "k");

  s.AddConstraint(s.MakeEquality(i, j));
  s.AddConstraint(s.MakeEquality(j, k));
  IntVar* const left = s.MakeIsLessOrEqualVar(s.MakeSum(i, j), k);
  IntVar* const right = s.MakeIsLessOrEqualVar(s.MakeSum(i, k), j);

  s.AddConstraint(s.MakeGreater(s.MakeSum(left, right), Zero()));

  // search decision
  DecisionBuilder* const db = s.MakePhase(i, j, k,
                                          Solver::CHOOSE_FIRST_UNBOUND,
                                          Solver::ASSIGN_MIN_VALUE);
  SearchMonitor* const log = s.MakeSearchLog(100000);

  s.Solve(db, log);  // go!
}


void SlowPropagation(int size) {
  LOG(INFO) << "SlowPropagation: size = " << size;
  Solver s("SlowPropagation");
  const int64 rmin = -(1 << size);
  const int64 rmax = 1 << size;
  IntVar* const i = s.MakeIntVar(rmin, rmax, "i");
  IntVar* const j = s.MakeIntVar(rmin, rmax, "j");
  s.AddConstraint(s.MakeGreater(i, j));
  s.AddConstraint(s.MakeLessOrEqual(i, j));

  DecisionBuilder* const db = s.MakePhase(i, j,
                                          Solver::CHOOSE_FIRST_UNBOUND,
                                          Solver::ASSIGN_MIN_VALUE);

  SearchMonitor* const log = s.MakeSearchLog(100000);

  s.Solve(db, log);
}
}  // namespace operations_research

int main(int argc, char **argv) {
  google::ParseCommandLineFlags(&argc, &argv, true);
  operations_research::DeepSearchTreeArith(FLAGS_size);
  operations_research::SlowPropagationArith(FLAGS_size);
  // operations_research::DeepSearchTree(FLAGS_size);
  // operations_research::SlowPropagation(FLAGS_size);
  return 0;
}
