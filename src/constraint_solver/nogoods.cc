// Copyright 2010-2014 Google
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


#include <string>
#include <vector>

#include "base/integral_types.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/stringprintf.h"
#include "base/stl_util.h"
#include "constraint_solver/constraint_solver.h"
#include "util/string_array.h"

namespace operations_research {

// ----- Base Class -----

void NoGoodManager::EnterSearch() { Init(); }

void NoGoodManager::BeginNextDecision(DecisionBuilder* const db) { Apply(); }

bool NoGoodManager::AcceptSolution() {
  Apply();
  return true;
}

NoGood* NoGoodManager::MakeNoGood() { return new NoGood(); }

// ----- Base class for NoGood terms -----

class NoGoodTerm {
 public:
  enum TermStatus {
    ALWAYS_TRUE,
    ALWAYS_FALSE,
    UNDECIDED
  };
  NoGoodTerm() {}
  virtual ~NoGoodTerm() {}

  virtual TermStatus Evaluate() const = 0;
  virtual void Refute() = 0;
  virtual std::string DebugString() const = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(NoGoodTerm);
};

namespace {
// ----- IntegerVariableNoGoodTerm -----

// IntVar == int64 specialization.
class IntegerVariableNoGoodTerm : public NoGoodTerm {
 public:
  IntegerVariableNoGoodTerm(IntVar* const var, int64 value, bool assign)
      : integer_variable_(var), value_(value), assign_(assign) {
    CHECK(integer_variable_ != nullptr);
  }

  virtual TermStatus Evaluate() const {
    if (!integer_variable_->Contains(value_)) {
      return assign_ ? ALWAYS_FALSE : ALWAYS_TRUE;
    } else if (integer_variable_->Bound()) {
      return assign_ ? ALWAYS_TRUE : ALWAYS_FALSE;
    } else {
      return UNDECIDED;
    }
  }

  virtual void Refute() {
    if (assign_) {
      integer_variable_->RemoveValue(value_);
    } else {
      integer_variable_->SetValue(value_);
    }
  }

  virtual std::string DebugString() const {
    return StringPrintf("(%s %s %lld)", integer_variable_->name().c_str(),
                        assign_ ? "==" : "!=", value_);
  }

  IntVar* integer_variable() const { return integer_variable_; }
  int64 value() const { return value_; }
  bool assign() const { return assign_; }

 private:
  IntVar* const integer_variable_;
  const int64 value_;
  const bool assign_;
  DISALLOW_COPY_AND_ASSIGN(IntegerVariableNoGoodTerm);
};
}  // namespace

// ----- NoGood -----

NoGood::~NoGood() { STLDeleteElements(&terms_); }

void NoGood::AddIntegerVariableEqualValueTerm(IntVar* const var, int64 value) {
  terms_.push_back(new IntegerVariableNoGoodTerm(var, value, true));
}

void NoGood::AddIntegerVariableNotEqualValueTerm(IntVar* const var,
                                                 int64 value) {
  terms_.push_back(new IntegerVariableNoGoodTerm(var, value, false));
}

bool NoGood::Apply(Solver* const solver) {
  NoGoodTerm* first_undecided = nullptr;
  for (int i = 0; i < terms_.size(); ++i) {
    switch (terms_[i]->Evaluate()) {
      case NoGoodTerm::ALWAYS_TRUE: { break; }
      case NoGoodTerm::ALWAYS_FALSE: { return false; }
      case NoGoodTerm::UNDECIDED: {
        if (first_undecided == nullptr) {
          first_undecided = terms_[i];
        } else {
          // more than one undecided, we cannot deduce anything.
          return true;
        }
        break;
      }
    }
  }
  if (first_undecided == nullptr && terms_.size() > 0) {
    VLOG(2) << "No Good " << DebugString() << " -> Fail";
    solver->Fail();
  }
  if (first_undecided != nullptr) {
    VLOG(2) << "No Good " << DebugString() << " -> Refute "
            << first_undecided->DebugString();
    first_undecided->Refute();
    return false;
  }
  return false;
}

std::string NoGood::DebugString() const {
  return StringPrintf("(%s)", JoinDebugStringPtr(terms_, " && ").c_str());
}

namespace {
// ----- NoGoodManager -----

// This implementation is very naive. It will be kept in the future as
// a reference point.

class NaiveNoGoodManager : public NoGoodManager {
 public:
  explicit NaiveNoGoodManager(Solver* const solver) : NoGoodManager(solver) {}
  virtual ~NaiveNoGoodManager() { Clear(); }

  virtual void Clear() { STLDeleteElements(&nogoods_); }

  virtual void Init() {}

  virtual void AddNoGood(NoGood* const nogood) { nogoods_.push_back(nogood); }

  virtual int NoGoodCount() const { return nogoods_.size(); }

  virtual void Apply() {
    Solver* const s = solver();
    for (int i = 0; i < nogoods_.size(); ++i) {
      nogoods_[i]->Apply(s);
    }
  }

  std::string DebugString() const {
    return StringPrintf("NaiveNoGoodManager(%d)", NoGoodCount());
  }

 private:
  std::vector<NoGood*> nogoods_;
};
}  // namespace

// ----- API -----

NoGoodManager* Solver::MakeNoGoodManager() {
  return RevAlloc(new NaiveNoGoodManager(this));
}

}  // namespace operations_research
