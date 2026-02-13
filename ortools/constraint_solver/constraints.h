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

#ifndef ORTOOLS_CONSTRAINT_SOLVER_CONSTRAINTS_H_
#define ORTOOLS_CONSTRAINT_SOLVER_CONSTRAINTS_H_

#include <algorithm>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

#include "absl/log/check.h"
#include "absl/strings/str_cat.h"
#include "ortools/constraint_solver/constraint_solver.h"

namespace operations_research {

class ActionDemon : public Demon {
 public:
  explicit ActionDemon(const Solver::Action& action) : action_(action) {
    CHECK(action != nullptr);
  }

  ~ActionDemon() override = default;

  void Run(Solver* solver) override;

 private:
  Solver::Action action_;
};

class ClosureDemon : public Demon {
 public:
  explicit ClosureDemon(const Solver::Closure& closure) : closure_(closure) {
    CHECK(closure != nullptr);
  }

  ~ClosureDemon() override = default;

  void Run(Solver* solver) override;

 private:
  Solver::Closure closure_;
};

class TrueConstraint : public Constraint {
 public:
  explicit TrueConstraint(Solver* s) : Constraint(s) {}
  ~TrueConstraint() override = default;

  void Post() override;
  void InitialPropagate() override;
  std::string DebugString() const override;

  void Accept(ModelVisitor* visitor) const override;
  IntVar* Var() override;
};

class FalseConstraint : public Constraint {
 public:
  explicit FalseConstraint(Solver* s) : Constraint(s) {}
  FalseConstraint(Solver* s, const std::string& explanation)
      : Constraint(s), explanation_(explanation) {}
  ~FalseConstraint() override = default;

  void Post() override;
  void InitialPropagate() override;
  std::string DebugString() const override;

  void Accept(ModelVisitor* visitor) const override;
  IntVar* Var() override;

 private:
  const std::string explanation_;
};

class MapDomain : public Constraint {
 public:
  MapDomain(Solver* s, IntVar* var, const std::vector<IntVar*>& actives)
      : Constraint(s), var_(var), actives_(actives) {
    holes_ = var->MakeHoleIterator(true);
  }

  ~MapDomain() override = default;

  void Post() override;

  void InitialPropagate() override;

  void UpdateActive(int64_t index);

  void VarDomain();

  void VarBound();
  std::string DebugString() const override;

  void Accept(ModelVisitor* visitor) const override;

 private:
  IntVar* var_;
  std::vector<IntVar*> actives_;
  IntVarIterator* holes_;
};

class LexicalLessOrEqual : public Constraint {
 public:
  LexicalLessOrEqual(Solver* s, std::vector<IntVar*> left,
                     std::vector<IntVar*> right, std::vector<int64_t> offsets)
      : Constraint(s),
        left_(std::move(left)),
        right_(std::move(right)),
        active_var_(0),
        offsets_(std::move(offsets)),
        demon_added_(offsets_.size(), false),
        demon_(nullptr) {
    CHECK_EQ(left_.size(), right_.size());
    CHECK_EQ(offsets_.size(), right_.size());
    CHECK(std::all_of(offsets_.begin(), offsets_.end(),
                      [](int step) { return step > 0; }));
  }

  ~LexicalLessOrEqual() override = default;

  void Post() override;

  void InitialPropagate() override;

  std::string DebugString() const override;

  void Accept(ModelVisitor* visitor) const override;

 private:
  int JumpEqualVariables(int start_position) const;
  void AddDemon(int position);

  std::vector<IntVar*> left_;
  std::vector<IntVar*> right_;
  NumericalRev<int> active_var_;
  std::vector<int64_t> offsets_;
  RevArray<bool> demon_added_;
  Demon* demon_;
};

class InversePermutationConstraint : public Constraint {
 public:
  InversePermutationConstraint(Solver* s, const std::vector<IntVar*>& left,
                               const std::vector<IntVar*>& right)
      : Constraint(s),
        left_(left),
        right_(right),
        left_hole_iterators_(left.size()),
        left_domain_iterators_(left_.size()),
        right_hole_iterators_(right_.size()),
        right_domain_iterators_(right_.size()) {
    CHECK_EQ(left_.size(), right_.size());
    for (int i = 0; i < left_.size(); ++i) {
      left_hole_iterators_[i] = left_[i]->MakeHoleIterator(true);
      left_domain_iterators_[i] = left_[i]->MakeDomainIterator(true);
      right_hole_iterators_[i] = right_[i]->MakeHoleIterator(true);
      right_domain_iterators_[i] = right_[i]->MakeDomainIterator(true);
    }
  }

  ~InversePermutationConstraint() override = default;

  void Post() override;

  void InitialPropagate() override;

  void PropagateHolesOfLeftVarToRight(int index);

  void PropagateHolesOfRightVarToLeft(int index);

  std::string DebugString() const override;

  void Accept(ModelVisitor* visitor) const override;

 private:
  // See PropagateHolesOfLeftVarToRight() and PropagateHolesOfRightVarToLeft().
  void PropagateHoles(int index, IntVar* var, IntVarIterator* holes,
                      const std::vector<IntVar*>& inverse);

  void PropagateDomain(int index, IntVar* var, IntVarIterator* domain,
                       const std::vector<IntVar*>& inverse);

  std::vector<IntVar*> left_;
  std::vector<IntVar*> right_;
  std::vector<IntVarIterator*> left_hole_iterators_;
  std::vector<IntVarIterator*> left_domain_iterators_;
  std::vector<IntVarIterator*> right_hole_iterators_;
  std::vector<IntVarIterator*> right_domain_iterators_;

  // used only in PropagateDomain().
  std::vector<int64_t> tmp_removed_values_;
};

class IndexOfFirstMaxValue : public Constraint {
 public:
  IndexOfFirstMaxValue(Solver* solver, IntVar* index,
                       const std::vector<IntVar*>& vars)
      : Constraint(solver), index_(index), vars_(vars) {}

  ~IndexOfFirstMaxValue() override = default;

  void Post() override;

  void InitialPropagate() override;

  std::string DebugString() const override;

  void Accept(ModelVisitor* visitor) const override;

 private:
  IntVar* index_;
  const std::vector<IntVar*> vars_;
};

#ifndef SWIG

/// @{
/// These methods represent generic demons that will call back a
/// method on the constraint during their Run method.
/// This way, all propagation methods are members of the constraint class,
/// and demons are just proxies with a priority of NORMAL_PRIORITY.

/// Demon proxy to a method on the constraint with no arguments.
template <class T>
class CallMethod0 : public Demon {
 public:
  CallMethod0(T* ct, void (T::*method)(), const std::string& name)
      : constraint_(ct), method_(method), name_(name) {}

  ~CallMethod0() override = default;

  void Run(Solver*) override { (constraint_->*method_)(); }

  std::string DebugString() const override {
    return "CallMethod_" + name_ + "(" + constraint_->DebugString() + ")";
  }

 private:
  T* constraint_;
  void (T::*method_)();
  const std::string name_;
};

template <class T>
Demon* MakeConstraintDemon0(Solver* s, T* ct, void (T::*method)(),
                            const std::string& name) {
  return s->RevAlloc(new CallMethod0<T>(ct, method, name));
}

template <class P>
std::string ParameterDebugString(P param) {
  return absl::StrCat(param);
}

/// Support limited to pointers to classes which define DebugString().
template <class P>
std::string ParameterDebugString(P* param) {
  return param->DebugString();
}

/// Demon proxy to a method on the constraint with one argument.
template <class T, class P>
class CallMethod1 : public Demon {
 public:
  CallMethod1(T* ct, void (T::*method)(P), const std::string& name, P param1)
      : constraint_(ct), method_(method), name_(name), param1_(param1) {}

  ~CallMethod1() override = default;

  void Run(Solver*) override { (constraint_->*method_)(param1_); }

  std::string DebugString() const override {
    return absl::StrCat("CallMethod_", name_, "(", constraint_->DebugString(),
                        ", ", ParameterDebugString(param1_), ")");
  }

 private:
  T* constraint_;
  void (T::*method_)(P);
  const std::string name_;
  P param1_;
};

template <class T, class P>
Demon* MakeConstraintDemon1(Solver* s, T* ct, void (T::*method)(P),
                            const std::string& name, P param1) {
  return s->RevAlloc(new CallMethod1<T, P>(ct, method, name, param1));
}

/// Demon proxy to a method on the constraint with two arguments.
template <class T, class P, class Q>
class CallMethod2 : public Demon {
 public:
  CallMethod2(T* ct, void (T::*method)(P, Q), const std::string& name, P param1,
              Q param2)
      : constraint_(ct),
        method_(method),
        name_(name),
        param1_(param1),
        param2_(param2) {}

  ~CallMethod2() override = default;

  void Run(Solver*) override { (constraint_->*method_)(param1_, param2_); }

  std::string DebugString() const override {
    return absl::StrCat("CallMethod_", name_, "(", constraint_->DebugString(),
                        ", ", ParameterDebugString(param1_), ", ",
                        ParameterDebugString(param2_), ")");
  }

 private:
  T* constraint_;
  void (T::*method_)(P, Q);
  const std::string name_;
  P param1_;
  Q param2_;
};

template <class T, class P, class Q>
Demon* MakeConstraintDemon2(Solver* s, T* ct, void (T::*method)(P, Q),
                            const std::string& name, P param1, Q param2) {
  return s->RevAlloc(
      new CallMethod2<T, P, Q>(ct, method, name, param1, param2));
}
/// Demon proxy to a method on the constraint with three arguments.
template <class T, class P, class Q, class R>
class CallMethod3 : public Demon {
 public:
  CallMethod3(T* ct, void (T::*method)(P, Q, R), const std::string& name,
              P param1, Q param2, R param3)
      : constraint_(ct),
        method_(method),
        name_(name),
        param1_(param1),
        param2_(param2),
        param3_(param3) {}

  ~CallMethod3() override = default;

  void Run(Solver*) override {
    (constraint_->*method_)(param1_, param2_, param3_);
  }

  std::string DebugString() const override {
    return absl::StrCat(absl::StrCat("CallMethod_", name_),
                        absl::StrCat("(", constraint_->DebugString()),
                        absl::StrCat(", ", ParameterDebugString(param1_)),
                        absl::StrCat(", ", ParameterDebugString(param2_)),
                        absl::StrCat(", ", ParameterDebugString(param3_), ")"));
  }

 private:
  T* constraint_;
  void (T::*method_)(P, Q, R);
  const std::string name_;
  P param1_;
  Q param2_;
  R param3_;
};

template <class T, class P, class Q, class R>
Demon* MakeConstraintDemon3(Solver* s, T* ct, void (T::*method)(P, Q, R),
                            const std::string& name, P param1, Q param2,
                            R param3) {
  return s->RevAlloc(
      new CallMethod3<T, P, Q, R>(ct, method, name, param1, param2, param3));
}
/// @}

/// @{
/// These methods represents generic demons that will call back a
/// method on the constraint during their Run method. This demon will
/// have a priority DELAYED_PRIORITY.

/// Low-priority demon proxy to a method on the constraint with no arguments.
template <class T>
class DelayedCallMethod0 : public Demon {
 public:
  DelayedCallMethod0(T* ct, void (T::*method)(), const std::string& name)
      : constraint_(ct), method_(method), name_(name) {}

  ~DelayedCallMethod0() override = default;

  void Run(Solver*) override { (constraint_->*method_)(); }

  Solver::DemonPriority priority() const override {
    return Solver::DELAYED_PRIORITY;
  }

  std::string DebugString() const override {
    return "DelayedCallMethod_" + name_ + "(" + constraint_->DebugString() +
           ")";
  }

 private:
  T* constraint_;
  void (T::*method_)();
  const std::string name_;
};

template <class T>
Demon* MakeDelayedConstraintDemon0(Solver* s, T* ct, void (T::*method)(),
                                   const std::string& name) {
  return s->RevAlloc(new DelayedCallMethod0<T>(ct, method, name));
}

/// Low-priority demon proxy to a method on the constraint with one argument.
template <class T, class P>
class DelayedCallMethod1 : public Demon {
 public:
  DelayedCallMethod1(T* ct, void (T::*method)(P), const std::string& name,
                     P param1)
      : constraint_(ct), method_(method), name_(name), param1_(param1) {}

  ~DelayedCallMethod1() override = default;

  void Run(Solver*) override { (constraint_->*method_)(param1_); }

  Solver::DemonPriority priority() const override {
    return Solver::DELAYED_PRIORITY;
  }

  std::string DebugString() const override {
    return absl::StrCat("DelayedCallMethod_", name_, "(",
                        constraint_->DebugString(), ", ",
                        ParameterDebugString(param1_), ")");
  }

 private:
  T* constraint_;
  void (T::*method_)(P);
  const std::string name_;
  P param1_;
};

template <class T, class P>
Demon* MakeDelayedConstraintDemon1(Solver* s, T* ct, void (T::*method)(P),
                                   const std::string& name, P param1) {
  return s->RevAlloc(new DelayedCallMethod1<T, P>(ct, method, name, param1));
}

/// Low-priority demon proxy to a method on the constraint with two arguments.
template <class T, class P, class Q>
class DelayedCallMethod2 : public Demon {
 public:
  DelayedCallMethod2(T* ct, void (T::*method)(P, Q), const std::string& name,
                     P param1, Q param2)
      : constraint_(ct),
        method_(method),
        name_(name),
        param1_(param1),
        param2_(param2) {}

  ~DelayedCallMethod2() override = default;

  void Run(Solver*) override { (constraint_->*method_)(param1_, param2_); }

  Solver::DemonPriority priority() const override {
    return Solver::DELAYED_PRIORITY;
  }

  std::string DebugString() const override {
    return absl::StrCat(absl::StrCat("DelayedCallMethod_", name_),
                        absl::StrCat("(", constraint_->DebugString()),
                        absl::StrCat(", ", ParameterDebugString(param1_)),
                        absl::StrCat(", ", ParameterDebugString(param2_), ")"));
  }

 private:
  T* constraint_;
  void (T::*method_)(P, Q);
  const std::string name_;
  P param1_;
  Q param2_;
};

template <class T, class P, class Q>
Demon* MakeDelayedConstraintDemon2(Solver* s, T* ct, void (T::*method)(P, Q),
                                   const std::string& name, P param1,
                                   Q param2) {
  return s->RevAlloc(
      new DelayedCallMethod2<T, P, Q>(ct, method, name, param1, param2));
}
/// @}

#endif  // !defined(SWIG)

}  // namespace operations_research

#endif  // ORTOOLS_CONSTRAINT_SOLVER_CONSTRAINTS_H_
