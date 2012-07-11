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
#include "base/hash.h"
#include "base/int-type-indexed-vector.h"
#include "base/int-type.h"
#include "base/integral_types.h"
#include "base/logging.h"
#include "base/map-util.h"
#include "base/scoped_ptr.h"
#include "base/stl_util.h"
#include "base/stringprintf.h"
#include "constraint_solver/constraint_solver.h"
#include "constraint_solver/constraint_solveri.h"
#include "util/bitset.h"
#include "util/const_int_array.h"

#include "core/Solver.cc"

namespace operations_research {
namespace {
DEFINE_INT_TYPE(AtomIndex, int);

const static AtomIndex kFailAtom = AtomIndex(0);

class SatPropagator : public Constraint {
 public:
  SatPropagator(Solver* const solver) : Constraint(solver) {}

  ~SatPropagator() {}

  bool Check(IntExpr* const expr) const {
    IntVar* expr_var = NULL;
    bool expr_negated = false;
    return solver()->IsBooleanVar(expr, &expr_var, &expr_negated);
  }

  bool Check(const std::vector<IntVar*>& vars) const {
    for (int i = 0; i < vars.size(); ++i) {
      if (!Check(vars[i])) {
        return false;
      }
    }
    return true;
  }

  AtomIndex Index(IntExpr* const expr) {
    IntVar* expr_var = NULL;
    bool expr_negated = false;
    if (!solver()->IsBooleanVar(expr, &expr_var, &expr_negated)) {
      return kFailAtom;
    }
    const AtomIndex index = AtomIndex(indices_.Add(expr_var));
    return expr_negated ? -index : index;
  }

  void VariableBound(int index) {
    // if (indices_[index]->Min() == 0) {
    //   Flip(AtomIndex(-1 - index));
    // } else {
    //   Flip(AtomIndex(1 + index));
    // }
  }

  virtual void Post() {
    for (int i = 0; i < indices_.size(); ++i) {
      Demon* const d = MakeConstraintDemon1(solver(),
                                            this,
                                            &SatPropagator::VariableBound,
                                            "VariableBound",
                                            i);
      indices_[i]->WhenDomain(d);
    }
  }

  virtual void InitialPropagate() {
    for (int i = 0; i < indices_.size(); ++i) {
      if (indices_[i]->Bound()) {
        VariableBound(i);
      }
    }
  }

 private:
  Minisat::Solver minisat_;
  VectorMap<IntVar*> indices_;
};
}  // namespace

bool AddBoolEq(SatPropagator* const sat, IntExpr* const left, IntExpr* const right) {
  if (!sat->Check(left) || !sat->Check(right)) {
    return false;
  }
  AtomIndex left_atom = sat->Index(left);
  AtomIndex right_atom = sat->Index(right);
  // sat->AddFlipAction(left_atom, right_atom);
  // sat->AddFlipAction(right_atom, left_atom);
  // sat->AddFlipAction(-left_atom, -right_atom);
  // sat->AddFlipAction(-right_atom, -left_atom);
  return true;
}

bool AddBoolLe(SatPropagator* const sat, IntExpr* const left, IntExpr* const right) {
  if (!sat->Check(left) || !sat->Check(right)) {
    return false;
  }
  AtomIndex left_atom = sat->Index(left);
  AtomIndex right_atom = sat->Index(right);
  // sat->AddFlipAction(left_atom, right_atom);
  // sat->AddFlipAction(-right_atom, -left_atom);
  return true;
}

bool AddBoolNot(SatPropagator* const sat, IntExpr* const left, IntExpr* const right) {
  if (!sat->Check(left) || !sat->Check(right)) {
    return false;
  }
  AtomIndex left_atom = sat->Index(left);
  AtomIndex right_atom = sat->Index(right);
  // sat->AddFlipAction(left_atom, -right_atom);
  // sat->AddFlipAction(right_atom, -left_atom);
  // sat->AddFlipAction(-left_atom, right_atom);
  // sat->AddFlipAction(-right_atom, left_atom);
  return true;
}

bool AddBoolAndArrayEqVar(SatPropagator* const sat,
                          const std::vector<IntVar*>& vars,
                          IntVar* const target) {
  return false;

}

bool AddBoolOrArrayEqualTrue(SatPropagator* const sat,
                             const std::vector<IntVar*>& vars) {
  if (!sat->Check(vars)) {
    return false;
  }
  std::vector<AtomIndex> atoms(vars.size());
  for (int i = 0; i < vars.size(); ++i) {
    atoms[i] = sat->Index(vars[i]);
  }
  return false;
}
}  // namespace operations_research
