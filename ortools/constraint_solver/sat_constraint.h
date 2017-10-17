// Copyright 2010-2017 Google
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

// This file contains the definition and implementation of a constraint
// encapsulating a full SAT solver. Such a constraint can basically propagates
// any relationship between Boolean variables that can be expressed using
// clauses or pseudo-Boolean constraint.
//
// It also contains some utility classes to map an IntVar to a set of Boolean
// variables. Using this, a lot of constraints on integer variables can be dealt
// with quite efficiently by just adding their encoding to the underlying sat
// solver (for instance a table constraint propagated this way should be really
// efficient).
//
// TODO(user): Extend so that the constraint solver can use the conflict
// learning mecanism present in the underlying SAT solver.

#ifndef OR_TOOLS_CONSTRAINT_SOLVER_SAT_CONSTRAINT_H_
#define OR_TOOLS_CONSTRAINT_SOLVER_SAT_CONSTRAINT_H_

#include <unordered_map>
#include <unordered_set>

#include "ortools/base/map_util.h"
#include "ortools/base/hash.h"
#include "ortools/constraint_solver/constraint_solver.h"
#include "ortools/constraint_solver/constraint_solveri.h"
#include "ortools/sat/sat_solver.h"
#include "ortools/util/tuple_set.h"

namespace operations_research {

// Given an IntVar, this class fetches the sat::Literal associated with
// the fact that the variable is equal or not to a given value in its domain.
class IntVarLiteralGetter {
 public:
  // The mapping of IntVar values to Boolean variables is as follow:
  // - We always assume the IntVar to take all possible values in [min, max].
  // - We create one Boolean variable per value: {i, i + 1, ..., i + max - min}.
  // - If size([min, max] == 2) then we just use one variable i that represent
  //   the fact that the variable is bound to its min value.
  //
  // TODO(user): Support holes in the interval.
  IntVarLiteralGetter(sat::BooleanVariable i, int64 min, int64 max)
      : first_variable_(i), min_value_(min), max_value_(max) {}

  // Returns the literal encoding (var == value).
  sat::Literal IsEqualTo(int64 value) const {
    if (max_value_ == min_value_ + 1) {
      return sat::Literal(first_variable_, value == min_value_);
    } else {
      return sat::Literal(first_variable_ + value - min_value_, true);
    }
  }

  // Returns the literal encoding (var != value).
  sat::Literal IsNotEqualTo(int64 value) const {
    return IsEqualTo(value).Negated();
  }

  // Returns the number of Boolean variables used for the encoding.
  int NumVariableUsed() const {
    return (max_value_ == min_value_ + 1) ? 1 : max_value_ - min_value_ + 1;
  }

 private:
  // These should really be const, but this causes problem with our open source
  // build because we use std::vector<IntVarLiteralGetter>.
  sat::BooleanVariable first_variable_;
  int64 min_value_;
  int64 max_value_;
};

// Creates a new set of Boolean variables for each registered IntVar. These
// variables will encode for each possible value, whether or not the IntVar is
// fixed to this value or not.
//
// This class also provide utility to:
// - Find the Boolean variable associated to an IntVar value.
// - Find the corresponding IntVar value from the Boolean variable.
class BooleanVariableManager {
 public:
  explicit BooleanVariableManager(sat::SatSolver* solver) : solver_(solver) {}

  // If not already done, this register the given IntVar with this manager and
  // creates the underlying Boolean variables in the SAT solver.
  // Returns the IntVar registration index.
  int RegisterIntVar(IntVar* int_var);

  // Returns the list of registered IntVar.
  // Note that they are ordered by registration index.
  const std::vector<IntVar*>& RegisteredIntVars() const {
    return registered_int_vars_;
  }

  // Returns the IntVarLiteralGetter associated with the IntVar of given
  // registration index. The reg_index can be retrieved with RegisterIntVar()
  // or by iterating over RegisteredIntVars(). It must be valid.
  const IntVarLiteralGetter& AssociatedBooleanVariables(int reg_index) const {
    return associated_variables_[reg_index];
  }

  // A Boolean variable associated to an IntVar value means (var == value) if
  // it is true. This returns the IntVar and the value. If the pointer is
  // nullptr, then this variable index wasn't created by this class.
  std::pair<IntVar*, int64> BooleanVariableMeaning(
      sat::BooleanVariable var) const {
    DCHECK_GE(var, 0);
    // This test is necessary because the SAT solver may know of variables not
    // registered by this class.
    if (var >= variable_meaning_.size()) return std::make_pair(nullptr, 0);
    return variable_meaning_[var];
  }

 private:
  sat::SatSolver* solver_;
  std::vector<IntVar*> registered_int_vars_;
  std::vector<IntVarLiteralGetter> associated_variables_;
  std::unordered_map<IntVar*, int> registration_index_map_;
  ITIVector<sat::BooleanVariable, std::pair<IntVar*, int64>> variable_meaning_;
  DISALLOW_COPY_AND_ASSIGN(BooleanVariableManager);
};

// The actual constraint encapsulating the SAT solver.
class SatConstraint : public Constraint {
 public:
  explicit SatConstraint(Solver* const solver)
      : Constraint(solver),
        variable_manager_(&sat_solver_),
        propagated_trail_index_(0),
        rev_decision_level_(0) {}

  // Register the demons.
  void Post() override {
    int i = 0;
    for (IntVar* int_var : variable_manager_.RegisteredIntVars()) {
      int_var->WhenDomain(MakeConstraintDemon1(
          solver(), this, &SatConstraint::Enqueue, "Enqueue", i));
      ++i;
    }
  }

  // Initial propagation.
  void InitialPropagate() override {
    if (sat_solver_.IsModelUnsat()) solver()->Fail();
    for (int i = 0; i < variable_manager_.RegisteredIntVars().size(); ++i) {
      Enqueue(i);
    }
  }

  // Returns the underlying solver and variable manager
  // These classes are used to create and add new constraint to the SAT solver.
  // Note that any mapping IntVar -> Boolean variable must be done with this
  // variable manager for the SatConstraint to properly push back to the
  // constraint solver the propagated Boolean variables.
  sat::SatSolver* SatSolver() { return &sat_solver_; }
  BooleanVariableManager* VariableManager() { return &variable_manager_; }

 private:
  // Push variable propagated from sat to the constraint solver.
  void PropagateFromSatToCp() {
    const sat::Trail& trail = sat_solver_.LiteralTrail();
    while (propagated_trail_index_ < trail.Index()) {
      const sat::Literal literal = trail[propagated_trail_index_];
      const sat::BooleanVariable var = literal.Variable();
      if (trail.AssignmentType(var) != sat::AssignmentType::kSearchDecision) {
        std::pair<IntVar*, int64> p =
            variable_manager_.BooleanVariableMeaning(var);
        IntVar* int_var = p.first;
        const int64 value = p.second;
        if (int_var != nullptr) {
          if (literal.IsPositive()) {
            int_var->SetValue(value);
          } else {
            int_var->RemoveValue(value);
          }
        }
      }
      ++propagated_trail_index_;
    }
  }

  // Called when more information is known on the IntVar with given registration
  // index in the BooleanVariableManager.
  void Enqueue(int reg_index) {
    if (sat_solver_.CurrentDecisionLevel() > rev_decision_level_.Value()) {
      // The constraint solver backtracked. Synchronise the state.
      sat_solver_.Backtrack(rev_decision_level_.Value());
      propagated_trail_index_ =
          std::min(propagated_trail_index_, sat_solver_.LiteralTrail().Index());
    }

    const IntVar* int_var = variable_manager_.RegisteredIntVars()[reg_index];
    const IntVarLiteralGetter literal_getter =
        variable_manager_.AssociatedBooleanVariables(reg_index);

    if (int_var->Bound()) {
      if (!EnqueueLiteral(literal_getter.IsEqualTo(int_var->Value()))) {
        solver()->Fail();
      }
    } else {
      const int64 vmin = int_var->Min();
      for (int64 value = int_var->OldMin(); value < vmin; ++value) {
        if (!EnqueueLiteral(literal_getter.IsNotEqualTo(value))) {
          solver()->Fail();
        }
      }

      // TODO(user): Investigate caching the hole iterator.
      std::unique_ptr<IntVarIterator> it(int_var->MakeHoleIterator(false));
      for (const int64 value : InitAndGetValues(it.get())) {
        if (!EnqueueLiteral(literal_getter.IsNotEqualTo(value))) {
          solver()->Fail();
        }
      }
      const int64 old_max = int_var->OldMax();
      for (int64 value = int_var->Max() + 1; value <= old_max; ++value) {
        if (!EnqueueLiteral(literal_getter.IsNotEqualTo(value))) {
          solver()->Fail();
        }
      }
    }

    // TODO(user): Use a constraint solver mechanism to just do that once after
    // all the possible Enqueue() have been processed? see delayed demon example
    // in expr_array.cc
    PropagateFromSatToCp();
    rev_decision_level_.SetValue(solver(), sat_solver_.CurrentDecisionLevel());
  }

  // Try to Enqueue the given literal on the sat_trail. Returns false in case of
  // conflict, true otherwise. Note that the literal is only enqueued if it is
  // not already set.
  bool EnqueueLiteral(sat::Literal literal) {
    if (sat_solver_.Assignment().LiteralIsFalse(literal)) return false;
    if (sat_solver_.Assignment().LiteralIsTrue(literal)) return true;
    if (sat_solver_.EnqueueDecisionIfNotConflicting(literal)) return true;
    return false;
  }

  sat::SatSolver sat_solver_;
  BooleanVariableManager variable_manager_;
  int propagated_trail_index_;
  Rev<int> rev_decision_level_;

  DISALLOW_COPY_AND_ASSIGN(SatConstraint);
};

class SatTableConstraint : public Constraint {
 public:
  // Note that we need to copy the arguments.
  SatTableConstraint(Solver* s, const std::vector<IntVar*>& vars,
                     const IntTupleSet& tuples)
      : Constraint(s), vars_(vars), tuples_(tuples), sat_constraint_(s) {}

  void Post() override;
  void InitialPropagate() override { sat_constraint_.InitialPropagate(); }

 private:
  const std::vector<IntVar*> vars_;
  const IntTupleSet tuples_;

  // TODO(user): share this between different constraint. We need to pay
  // attention and call Post()/InitialPropagate() after all other constraint
  // have been posted though.
  SatConstraint sat_constraint_;

  DISALLOW_COPY_AND_ASSIGN(SatTableConstraint);
};

inline Constraint* BuildSatTableConstraint(Solver* solver,
                                           const std::vector<IntVar*>& vars,
                                           const IntTupleSet& tuples) {
  return solver->RevAlloc(new SatTableConstraint(solver, vars, tuples));
}

}  // namespace operations_research

#endif  // OR_TOOLS_CONSTRAINT_SOLVER_SAT_CONSTRAINT_H_
