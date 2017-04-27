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

#include "ortools/flatzinc/solver_util.h"

#include "ortools/base/stringprintf.h"
#include "ortools/base/join.h"
#include "ortools/flatzinc/logging.h"

namespace operations_research {
namespace fz {

void MarkComputedVariables(Constraint* ct,
                           std::unordered_set<IntegerVariable*>* marked) {
  const std::string& id = ct->type;
  if (id == "global_cardinality") {
    FZVLOG << "  - marking " << ct->DebugString() << FZENDL;
    for (IntegerVariable* const var : ct->arguments[2].variables) {
      marked->insert(var);
    }
  }

  if (id == "array_var_int_element" && ct->target_variable == nullptr) {
    FZVLOG << "  - marking " << ct->DebugString() << FZENDL;
    marked->insert(ct->arguments[2].Var());
  }

  if (id == "maximum_int" && ct->arguments[0].IsVariable() &&
      ct->target_variable == nullptr) {
    marked->insert(ct->arguments[0].Var());
  }

  if (id == "minimum_int" && ct->arguments[0].IsVariable() &&
      ct->target_variable == nullptr) {
    marked->insert(ct->arguments[0].Var());
  }

  if (id == "int_lin_eq" && ct->target_variable == nullptr) {
    const std::vector<int64>& array_coefficients = ct->arguments[0].values;
    const int size = array_coefficients.size();
    const std::vector<IntegerVariable*>& array_variables =
        ct->arguments[1].variables;
    bool todo = true;
    if (size == 0) {
      return;
    }
    if (array_coefficients[0] == -1) {
      for (int i = 1; i < size; ++i) {
        if (array_coefficients[i] < 0) {
          todo = false;
          break;
        }
      }
      // Can mark the first one, this is a hidden sum.
      if (todo) {
        marked->insert(array_variables[0]);
        FZVLOG << "  - marking " << ct->DebugString() << ": "
               << array_variables[0]->DebugString() << FZENDL;
        return;
      }
    }
    todo = true;
    if (array_coefficients[0] == 1) {
      for (int i = 1; i < size; ++i) {
        if (array_coefficients[i] > 0) {
          todo = false;
          break;
        }
      }
      if (todo) {
        marked->insert(array_variables[0]);
        FZVLOG << "  - marking " << ct->DebugString() << ": "
               << array_variables[0]->DebugString() << FZENDL;
        return;
      }
    }
    todo = true;
    if (array_coefficients[size - 1] == 1) {
      for (int i = 0; i < size - 1; ++i) {
        if (array_coefficients[i] > 0) {
          todo = false;
          break;
        }
      }
      if (todo) {
        // Can mark the last one, this is a hidden sum.
        marked->insert(array_variables[size - 1]);
        FZVLOG << "  - marking " << ct->DebugString() << ": "
               << array_variables[size - 1]->DebugString() << FZENDL;
        return;
      }
    }
    todo = true;
    if (array_coefficients[size - 1] == -1) {
      for (int i = 0; i < size - 1; ++i) {
        if (array_coefficients[i] < 0) {
          todo = false;
          break;
        }
      }
      if (todo) {
        // Can mark the last one, this is a hidden sum.
        marked->insert(array_variables[size - 1]);
        FZVLOG << "  - marking " << ct->DebugString() << ": "
               << array_variables[size - 1]->DebugString() << FZENDL;
        return;
      }
    }
  }
}

Interrupt::Interrupt(operations_research::Solver* const solver)
    : SearchLimit(solver) {}

Interrupt::~Interrupt() {}

bool Interrupt::Check() { return control_c_; }

void Interrupt::Init() {}

void Interrupt::Copy(const SearchLimit* const limit) {}

SearchLimit* Interrupt::MakeClone() const {
  return solver()->RevAlloc(new Interrupt(solver()));
}

bool Interrupt::control_c_ = false;

namespace {
// Comparison helpers.
struct VarDegreeIndexSize {
  IntVar* v;
  int d;
  int i;
  int b;

  VarDegreeIndexSize(IntVar* var, int degree, int index, uint64 size)
      : v(var), d(degree), i(index), b(Bucket(size)) {}

  int Bucket(uint64 size) const {
    if (size < 10) {
      return 0;
    } else if (size < 1000) {
      return 1;
    } else if (size < 100000) {
      return 2;
    } else {
      return 3;
    }
  }

  bool operator<(const VarDegreeIndexSize& other) const {
    return b < other.b ||
           (b == other.b && (d > other.d || (d == other.d && i < other.i)));
  }
};
}  // namespace

void SortVariableByDegree(const std::vector<int>& occurrences, bool use_size,
                          std::vector<IntVar*>* int_vars) {
  std::vector<VarDegreeIndexSize> to_sort;
  for (int i = 0; i < int_vars->size(); ++i) {
    IntVar* const var = (*int_vars)[i];
    const uint64 size = use_size ? var->Size() : 1;
    to_sort.push_back(VarDegreeIndexSize(var, occurrences[i], i, size));
  }
  std::sort(to_sort.begin(), to_sort.end());
  for (int i = 0; i < int_vars->size(); ++i) {
    (*int_vars)[i] = to_sort[i].v;
  }
}

// Report memory usage in a nice way.
// TODO(user): Move to open source base directory.
std::string MemoryUsage() {
  static const int64 kDisplayThreshold = 2;
  static const int64 kKiloByte = 1024;
  static const int64 kMegaByte = kKiloByte * kKiloByte;
  static const int64 kGigaByte = kMegaByte * kKiloByte;
  const int64 memory_usage = operations_research::Solver::MemoryUsage();
  if (memory_usage > kDisplayThreshold * kGigaByte) {
    return StringPrintf("%.2lf GB", memory_usage * 1.0 / kGigaByte);
  } else if (memory_usage > kDisplayThreshold * kMegaByte) {
    return StringPrintf("%.2lf MB", memory_usage * 1.0 / kMegaByte);
  } else if (memory_usage > kDisplayThreshold * kKiloByte) {
    return StringPrintf("%2lf KB", memory_usage * 1.0 / kKiloByte);
  } else {
    return StrCat(memory_usage);
  }
}

// Flatten Search annotations.
void FlattenAnnotations(const Annotation& ann, std::vector<Annotation>* out) {
  if (ann.type == Annotation::ANNOTATION_LIST ||
      ann.IsFunctionCallWithIdentifier("seq_search")) {
    for (const Annotation& inner : ann.annotations) {
      FlattenAnnotations(inner, out);
    }
  } else {
    out->push_back(ann);
  }
}

void Interrupt::ControlCHandler(int s) {
  FZLOG << "Ctrl-C caught" << FZENDL;
  operations_research::fz::Interrupt::control_c_ = true;
}

}  // namespace fz
}  // namespace operations_research
