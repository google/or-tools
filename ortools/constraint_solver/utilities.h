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

#ifndef ORTOOLS_CONSTRAINT_SOLVER_UTILITIES_H_
#define ORTOOLS_CONSTRAINT_SOLVER_UTILITIES_H_

#include <cstdint>
#include <limits>
#include <vector>

#include "absl/log/check.h"

namespace operations_research {

inline int64_t PosIntDivUp(int64_t e, int64_t v) {
  DCHECK_GT(v, 0);
  return (e < 0 || e % v == 0) ? e / v : e / v + 1;
}

inline int64_t PosIntDivDown(int64_t e, int64_t v) {
  DCHECK_GT(v, 0);
  return (e >= 0 || e % v == 0) ? e / v : e / v - 1;
}

template <class ValType, class VarType>
bool IsArrayInRange(const std::vector<VarType*>& vars, ValType min,
                    ValType max) {
  for (VarType* var : vars) {
    if (var->Min() < min || var->Max() > max) return false;
  }
  return true;
}

template <class VarType>
int64_t MinVarArray(const std::vector<VarType*>& vars) {
  int64_t min_val = std::numeric_limits<int64_t>::max();
  for (VarType* var : vars) {
    min_val = std::min(min_val, var->Min());
  }
  return min_val;
}

template <class VarType>
int64_t MaxVarArray(const std::vector<VarType*>& vars) {
  int64_t max_val = std::numeric_limits<int64_t>::min();
  for (VarType* var : vars) {
    max_val = std::max(max_val, var->Max());
  }
  return max_val;
}

template <class VarType>
bool AreAllBound(const std::vector<VarType*>& vars) {
  for (VarType* var : vars) {
    if (!var->Bound()) return false;
  }
  return true;
}

template <class VarType>
void FillValues(const std::vector<VarType*>& vars,
                std::vector<int64_t>* values) {
  values->clear();
  values->reserve(vars.size());
  for (VarType* var : vars) {
    values->push_back(var->Value());
  }
}

template <class T>
bool AreAllOnes(const std::vector<T>& values) {
  for (const T value : values) {
    if (value != 1) return false;
  }
  return true;
}

template <class T>
bool IsIncreasingContiguous(const std::vector<T>& values) {
  if (values.empty()) return true;
  for (int i = 0; i < values.size() - 1; ++i) {
    if (values[i + 1] != values[i] + 1) return false;
  }
  return true;
}

template <class T>
bool IsArrayConstant(const std::vector<T>& values, const T& value) {
  for (const T v : values) {
    if (v != value) return false;
  }
  return true;
}

template <class T>
bool IsArrayBoolean(const std::vector<T>& values) {
  for (const T value : values) {
    if (value != 0 && value != 1) return false;
  }
  return true;
}

template <class T>
bool IsIncreasing(const std::vector<T>& values) {
  if (values.empty()) return true;
  for (int i = 0; i < values.size() - 1; ++i) {
    if (values[i + 1] < values[i]) return false;
  }
  return true;
}

template <class T>
bool AreAllNull(const std::vector<T>& values) {
  for (const T value : values) {
    if (value != 0) return false;
  }
  return true;
}

template <class VarType, class T>
bool AreAllBoundOrNull(const std::vector<VarType*>& vars,
                       const std::vector<T>& values) {
  if (values.size() != vars.size()) return false;
  for (int i = 0; i < vars.size(); ++i) {
    if (values[i] != 0 && !vars[i]->Bound()) return false;
  }
  return true;
}

template <class VarType>
bool AreAllBooleans(const std::vector<VarType*>& vars) {
  for (VarType* var : vars) {
    if (var->Min() < 0 || var->Max() > 1) return false;
  }
  return true;
}

template <class T>
bool AreAllPositive(const std::vector<T>& values) {
  for (const T value : values) {
    if (value < 0) return false;
  }
  return true;
}

template <class T>
bool AreAllNegative(const std::vector<T>& values) {
  for (const T value : values) {
    if (value > 0) return false;
  }
  return true;
}

std::vector<int64_t> ToInt64Vector(const std::vector<int>& input);

// Calls the given method with the provided arguments on all objects in the
// collection.
template <typename T, typename MethodPointer, typename... Args>
void ForAll(const std::vector<T*>& objects, MethodPointer method,
            const Args&... args) {
  for (T* const object : objects) {
    DCHECK(object != nullptr);
    (object->*method)(args...);
  }
}

}  // namespace operations_research

#endif  // ORTOOLS_CONSTRAINT_SOLVER_UTILITIES_H_
