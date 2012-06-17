// Copyright 2010-2012 Google
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

#include <math.h>
#include <string.h>
#include <algorithm>
#include "base/hash.h"
#include <string>
#include <utility>
#include <vector>

#include "base/commandlineflags.h"
#include "base/integral_types.h"
#include "base/logging.h"
#include "base/scoped_ptr.h"
#include "base/stringprintf.h"
#include "base/map-util.h"
#include "constraint_solver/constraint_solver.h"
#include "constraint_solver/constraint_solveri.h"
#include "util/bitset.h"
#include "util/const_int_array.h"

namespace operations_research {
class SetVar : public PropagationBaseObject {
 public:
  SetVar(Solver* const s, int64 min_value, int64 max_value);
  SetVar(Solver* const s, const std::vector<int64>& values);
  SetVar(Solver* const s, const std::vector<int>& values);
  virtual ~SetVar();

  IntVar* Var(int64 value) const;
  IntVar* CardVar() const;

  virtual string DebugString() const;

  int64 SetMin() const;
  int64 SetMax() const;

 private:
  const int64 min_value_;
  const int64 max_value_;
  std::vector<IntVar*> elements_;
  IntVar* card_var_;
};

namespace {
template <class T> int64 MinValue(const std::vector<T>& values) {
  int64 result = kint64max;
  for (int i = 0; i < values.size(); ++i) {
    result = std::min(result, static_cast<int64>(values[i]));
  }
  return result;
}
template <class T> int64 MaxValue(const std::vector<T>& values) {
  int64 result = kint64min;
  for (int i = 0; i < values.size(); ++i) {
    result = std::max(result, static_cast<int64>(values[i]));
  }
  return result;
}
}  // namespace

SetVar::SetVar(Solver* const s, int64 min_value, int64 max_value)
    : PropagationBaseObject(s),
      min_value_(min_value),
      max_value_(max_value) {
  const int64 size = max_value_ - min_value_ + 1;
  elements_.resize(size);
  for (int i = 0; i < size; ++i) {
    elements_[i] = s->MakeBoolVar();
  }
  card_var_ = s->MakeSum(elements_)->Var();
}

SetVar::SetVar(Solver* const s, const std::vector<int64>& values)
    : PropagationBaseObject(s),
      min_value_(MinValue(values)),
      max_value_(MaxValue(values)) {
  const int64 size = max_value_ - min_value_ + 1;
  elements_.resize(size);
  hash_set<int64> set_of_values(values.begin(), values.end());
  for (int i = 0; i < size; ++i) {
    if (ContainsKey(set_of_values, i + min_value_)) {
      elements_[i] = s->MakeBoolVar();
    } else {
      elements_[i] = s->MakeIntConst(0);
    }
  }
  card_var_ = s->MakeSum(elements_)->Var();
}

SetVar::SetVar(Solver* const s, const std::vector<int>& values)
    : PropagationBaseObject(s),
      min_value_(MinValue(values)),
      max_value_(MaxValue(values)) {
  const int size = max_value_ - min_value_ + 1;
  elements_.resize(size);
  hash_set<int> set_of_values(values.begin(), values.end());
  for (int i = 0; i < size; ++i) {
    if (ContainsKey(set_of_values, i + min_value_)) {
      elements_[i] = s->MakeBoolVar();
    } else {
      elements_[i] = s->MakeIntConst(0);
    }
  }
  card_var_ = s->MakeSum(elements_)->Var();
}

IntVar* SetVar::Var(int64 index) const {
  DCHECK_GE(index, min_value_);
  DCHECK_LE(index, max_value_);
  return elements_[index - min_value_];
}

IntVar* SetVar::CardVar() const {
  return card_var_;
}

int64 SetVar::SetMin() const {
  return min_value_;
}

int64 SetVar::SetMax() const {
  return max_value_;
}

string SetVar::DebugString() const {
  string result = StringPrintf("SetVar(card = %s, values = [",
                               card_var_->DebugString().c_str());
  for (int i = 0; i < elements_.size(); ++i) {
    if (elements_[i]->Max() == 1) {
      if (elements_[i]->Min() == 1) {
        result += StringPrintf("%" GG_LL_FORMAT "d", i + min_value_);
      } else {
        result += StringPrintf("?%" GG_LL_FORMAT "d", i + min_value_);
      }
    }
  }
  result += "])";
  return result;
}

}  // namespace operations_research
