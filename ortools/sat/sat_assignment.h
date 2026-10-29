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

// Assignment of values to variables.

#ifndef ORTOOLS_SAT_SAT_ASSIGNMENT_H_
#define ORTOOLS_SAT_SAT_ASSIGNMENT_H_

#include <cstdint>
#include <string>

#include "absl/log/check.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_format.h"
#include "ortools/sat/sat_literal.h"
#include "ortools/util/bitset.h"
#include "ortools/util/strong_integers.h"

namespace operations_research {
namespace sat {

// Holds the current variable assignment of the solver.
// Each variable can be unassigned or be assigned to true or false.
class VariablesAssignment {
 private:
  template <typename ViewType>
  class ViewImpl {
   public:
    explicit ViewImpl(const VariablesAssignment& assignment)
        : view_(assignment.assignment_.const_view()) {}
    explicit ViewImpl(VariablesAssignment* assignment)
        : view_(assignment->assignment_.view()) {}

    bool VariableIsAssigned(BooleanVariable var) const {
      return LiteralIsAssigned(Literal(var, true));
    }
    bool LiteralIsTrue(Literal literal) const { return view_[literal.Index()]; }
    bool LiteralIsFalse(Literal literal) const {
      return view_[literal.NegatedIndex()];
    }
    bool LiteralIsAssigned(Literal literal) const {
      return LiteralIsTrue(literal) || LiteralIsFalse(literal);
    }
    void AssignFromTrueLiteral(Literal literal) {
      DCHECK(!VariableIsAssigned(literal.Variable()));
      view_.Set(literal.Index());
    }
    void UnassignLiteral(Literal literal) {
      DCHECK(VariableIsAssigned(literal.Variable()));
      view_.Clear(literal.Index());
      view_.Clear(literal.NegatedIndex());
    }

   private:
    ViewType view_;
  };

 public:
  using ConstView = ViewImpl<Bitset64<LiteralIndex>::ConstView>;
  using View = ViewImpl<Bitset64<LiteralIndex>::View>;

  VariablesAssignment() = default;
  explicit VariablesAssignment(int num_variables) { Resize(num_variables); }

  // This type is neither copyable nor movable.
  VariablesAssignment(const VariablesAssignment&) = delete;
  VariablesAssignment& operator=(const VariablesAssignment&) = delete;
  void Resize(int num_variables) {
    assignment_.Resize(LiteralIndex(num_variables << 1));
  }

  // Makes the given literal true by assigning its underlying variable to either
  // true or false depending on the literal sign. This can only be called on an
  // unassigned variable.
  void AssignFromTrueLiteral(Literal literal) {
    DCHECK(!VariableIsAssigned(literal.Variable()));
    assignment_.Set(literal.Index());
  }

  // Unassigns the variable corresponding to the given literal.
  // This can only be called on an assigned variable.
  void UnassignLiteral(Literal literal) {
    DCHECK(VariableIsAssigned(literal.Variable()));
    assignment_.Clear(literal.Index());
    assignment_.Clear(literal.NegatedIndex());
  }

  // Literal getters. Note that both can be false, in which case the
  // corresponding variable is not assigned.
  bool LiteralIsFalse(Literal literal) const {
    return assignment_.IsSet(literal.NegatedIndex());
  }
  bool LiteralIsTrue(Literal literal) const {
    return assignment_.IsSet(literal.Index());
  }
  bool LiteralIsAssigned(Literal literal) const {
    return LiteralIsTrue(literal) || LiteralIsFalse(literal);
  }

  // Returns true iff the given variable is assigned.
  bool VariableIsAssigned(BooleanVariable var) const {
    return LiteralIsAssigned(Literal(var, true));
  }

  // Returns the literal of the given variable that is assigned to true.
  // That is, depending on the variable, it can be the positive literal or the
  // negative one. Only call this on an assigned variable.
  Literal GetTrueLiteralForAssignedVariable(BooleanVariable var) const {
    DCHECK(VariableIsAssigned(var));
    return Literal(var, assignment_.IsSet(LiteralIndex(var.value() << 1)));
  }

  int NumberOfVariables() const { return assignment_.size().value() / 2; }

  // Expose internal for performance critical code.
  // You should not use this in normal code.
  View GetMutableView() { return View(this); }
  ConstView GetConstView() const { return ConstView(*this); }

  std::string DebugString() const {
    std::string result;
    for (int i = 0; i < NumberOfVariables(); ++i) {
      const BooleanVariable var(i);
      if (VariableIsAssigned(var)) {
        absl::StrAppend(&result, LiteralIsTrue(Literal(var, true)) ? "1" : "0");
      } else {
        absl::StrAppend(&result, "?");
      }
    }
    return result;
  }

 private:
  // The encoding is as follows:
  // - assignment_.IsSet(literal.Index()) means literal is true.
  // - assignment_.IsSet(literal.Index() ^ 1]) means literal is false.
  // - If both are false, then the variable (and the literal) is unassigned.
  Bitset64<LiteralIndex> assignment_;
};

// For "hot" loop, it is better not to reload the Bitset64 pointer on each
// check.
using AssignmentView = VariablesAssignment::ConstView;
using MutableAssignmentView = VariablesAssignment::View;

// Information about a variable assignment.
struct AssignmentInfo {
  // The decision level at which this assignment was made. This starts at 0 and
  // increases each time the solver takes a search decision.
  //
  // TODO(user): We may be able to get rid of that for faster enqueues. Most of
  // the code only needs to know if this is 0 or the highest level, and for the
  // LBD computation, the literals of the conflict are already ordered by level,
  // so we could do it fairly efficiently.
  //
  // TODO(user): We currently don't support more than 2^28 decision levels. That
  // should be enough for most practical problems, but we should fail properly
  // if this limit is reached.
  uint32_t level : 28;

  // The type of assignment (see AssignmentType below).
  //
  // NOTE(user): We currently don't support more than 16 types of assignment.
  // This is checked in RegisterPropagator().
  mutable uint32_t type : 4;

  // The index of this assignment in the trail.
  int32_t trail_index;

  std::string DebugString() const {
    return absl::StrFormat("level:%d type:%d trail_index:%d", level, type,
                           trail_index);
  }
};
static_assert(sizeof(AssignmentInfo) == 8,
              "ERROR_AssignmentInfo_is_not_well_compacted");

}  // namespace sat
}  // namespace operations_research

#endif  // ORTOOLS_SAT_SAT_ASSIGNMENT_H_
