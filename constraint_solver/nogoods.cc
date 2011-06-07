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

#include "base/callback.h"
#include "base/integral_types.h"
#include "base/logging.h"
#include "base/scoped_ptr.h"
#include "base/stringprintf.h"
#include "base/concise_iterator.h"
#include "base/stl_util-inl.h"
#include "constraint_solver/constraint_solveri.h"

namespace operations_research {

// ----- Base Class -----

void NoGoodManager::EnterSearch() {
  Init();
}

void NoGoodManager::BeginNextDecision(DecisionBuilder* const db) {
  Apply();
}

bool NoGoodManager::AcceptSolution() {
  Apply();
  return true;
}

NoGood* NoGoodManager::MakeNoGood() {
  return new NoGood();
}

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
  virtual void ApplyReverse() = 0;
  virtual string DebugString() const = 0;
 private:
  DISALLOW_COPY_AND_ASSIGN(NoGoodTerm);
};

// ----- IntegerVariableNoGoodTerm -----

// IntVar == int64 specialization.
class IntegerVariableNoGoodTerm : public NoGoodTerm {
 public:
  IntegerVariableNoGoodTerm(IntVar* const var, int64 value, bool assign)
      : integer_variable_(var), value_(value), assign_(assign) {
    CHECK_NOTNULL(integer_variable_);
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

  virtual void ApplyReverse() {
    if (assign_) {
      integer_variable_->RemoveValue(value_);
    } else {
      integer_variable_->SetValue(value_);
    }
  }

  virtual string DebugString() const {
    return StringPrintf("(%s %s %lld)",
                        integer_variable_->name().c_str(),
                        assign_ ? "==" : "!=",
                        value_);
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

// ----- NoGood -----

NoGood::~NoGood() {
  STLDeleteElements(&terms_);
}

void NoGood::AddIntegerVariableEqualValueTerm(IntVar* const var, int64 value) {
  terms_.push_back(new IntegerVariableNoGoodTerm(var, value, true));
}

void NoGood::AddIntegerVariableNotEqualValueTerm(IntVar* const var,
                                                 int64 value) {
  terms_.push_back(new IntegerVariableNoGoodTerm(var, value, false));
}

void NoGood::Apply(Solver* const solver) {
  NoGoodTerm* first_undecided = NULL;
  for (int i = 0; i < terms_.size(); ++i) {
    switch (terms_[i]->Evaluate()) {
      case NoGoodTerm::ALWAYS_TRUE: {
        break;
      }
      case NoGoodTerm::ALWAYS_FALSE: {
        return;
      }
      case NoGoodTerm::UNDECIDED: {
        if (first_undecided == NULL) {
          first_undecided = terms_[i];
        } else {
          // more than one undecided, we cannot deduce anything.
          return;
        }
        break;
      }
    }
  }
  if (first_undecided == NULL && terms_.size() > 0) {
    solver->Fail();
  }
  if (first_undecided != NULL) {
    first_undecided->ApplyReverse();
  }
}

string NoGood::DebugString() const {
  string result = "(";
  for (int i = 0; i < terms_.size(); ++i) {
    if (i != 0) {
      result.append(" && ");
    }
    result.append(terms_[i]->DebugString());
  }
  result.append(")");
  return result;
}

// ----- NoGoodManager -----

// This implementation is very naive. It will be kept in the future as
// a reference point.

class NaiveNoGoodManager : public NoGoodManager {
 public:
  explicit NaiveNoGoodManager(Solver* const solver) : NoGoodManager(solver) {}
  virtual ~NaiveNoGoodManager() {
    Clear();
  }

  virtual void Clear() {
    STLDeleteElements(&nogoods_);
  }

  virtual void Init() {}

  virtual void AddNoGood(NoGood* const nogood) {
    nogoods_.push_back(nogood);
  }

  virtual int NoGoodCount() const {
    return nogoods_.size();
  }

  virtual void Apply() {
    Solver* const s = solver();
    for (int i = 0; i < nogoods_.size(); ++i) {
      nogoods_[i]->Apply(s);
    }
  }

  string DebugString() const {
    return StringPrintf("NaiveNoGoodManager(%d)", NoGoodCount());
  }

 private:
  std::vector<NoGood*> nogoods_;
};

// ----- API -----

NoGoodManager* Solver::MakeNoGoodManager() {
  return RevAlloc(new NaiveNoGoodManager(this));
}

}  // namespace operations_research
