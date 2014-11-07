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

// Basic types and classes used by the sat solver.

#ifndef OR_TOOLS_SAT_SAT_BASE_H_
#define OR_TOOLS_SAT_SAT_BASE_H_

#include "base/unique_ptr.h"
#include <string>
#include <vector>

#include "base/stringprintf.h"
#include "base/int_type_indexed_vector.h"
#include "base/int_type.h"
#include "util/bitset.h"

namespace operations_research {
namespace sat {

// Index of a variable (>= 0).
DEFINE_INT_TYPE(VariableIndex, int);

// Index of a literal (>= 0), see Literal below.
DEFINE_INT_TYPE(LiteralIndex, int);

// A literal is used to represent a variable or its negation. If it represents
// the variable it is said to be positive. If it represent its negation, it is
// said to be negative. We support two representations as an integer.
//
// The "signed" encoding of a literal is convenient for input/output and is used
// in the cnf file format. For a 0-based variable index x, (x + 1) represent the
// variable x and -(x + 1) represent its negation. The signed value 0 is an
// undefined literal and this class can never contain it.
//
// The "index" encoding of a literal is convenient as an index to an array
// and is the one used internally for efficiency. It is always positive or zero,
// and for a 0-based variable index x, (x << 1) encode the variable x and the
// same number XOR 1 encode its negation.
class Literal {
 public:
  // Not explicit for tests so we can write:
  // std::vector<literal> literal = {+1, -3, +4, -9};
  Literal(int signed_value)  // NOLINT
      : index_(signed_value > 0 ? ((signed_value - 1) << 1)
                                : ((-signed_value - 1) << 1) ^ 1) {
    CHECK_NE(signed_value, 0);
  }

  Literal() {}
  explicit Literal(LiteralIndex index) : index_(index.value()) {}
  Literal(VariableIndex variable, bool is_positive)
      : index_(is_positive ? (variable.value() << 1)
                           : (variable.value() << 1) ^ 1) {}

  VariableIndex Variable() const { return VariableIndex(index_ >> 1); }
  bool IsPositive() const { return !(index_ & 1); }
  bool IsNegative() const { return (index_ & 1); }

  LiteralIndex Index() const { return LiteralIndex(index_); }
  LiteralIndex NegatedIndex() const { return LiteralIndex(index_ ^ 1); }

  int SignedValue() const {
    return (index_ & 1) ? -((index_ >> 1) + 1) : ((index_ >> 1) + 1);
  }

  Literal Negated() const { return Literal(NegatedIndex()); }

  std::string DebugString() const { return StringPrintf("%+d", SignedValue()); }
  bool operator==(Literal other) const { return index_ == other.index_; }
  bool operator!=(Literal other) const { return index_ != other.index_; }

  bool operator<(const Literal& literal) const {
    return Index() < literal.Index();
  }

 private:
  int index_;
};

inline std::ostream& operator<<(std::ostream& os, Literal literal) {
  os << literal.DebugString();
  return os;
}

// Holds the current variable assignment of the solver.
// Each variables can be unassigned or be assigned to true or false.
class VariablesAssignment {
 public:
  VariablesAssignment() {}
  void Resize(int num_variables) {
    assignment_.Resize(LiteralIndex(num_variables << 1));
  }

  // Makes the given literal true by assigning its underlying variable to either
  // true or false depending on the literal sign. This can only be called on an
  // unassigned variable.
  void AssignFromTrueLiteral(Literal literal) {
    DCHECK(!IsVariableAssigned(literal.Variable()));
    assignment_.Set(literal.Index());
  }

  // Unassign the variable corresponding to the given literal.
  // This can only be called on an assigned variable.
  void UnassignLiteral(Literal literal) {
    DCHECK(IsVariableAssigned(literal.Variable()));
    assignment_.ClearTwoBits(literal.Index());
  }

  // Literal getters. Note that both can be false in which case the
  // corresponding variable is not assigned.
  bool IsLiteralFalse(Literal literal) const {
    return assignment_.IsSet(literal.NegatedIndex());
  }
  bool IsLiteralTrue(Literal literal) const {
    return assignment_.IsSet(literal.Index());
  }
  bool IsLiteralAssigned(Literal literal) const {
    return assignment_.AreOneOfTwoBitsSet(literal.Index());
  }

  // Returns true iff the given variable is assigned.
  bool IsVariableAssigned(VariableIndex var) const {
    return assignment_.AreOneOfTwoBitsSet(LiteralIndex(var.value() << 1));
  }

  // Returns the literal of the given variable that is assigned to true.
  // That is depending on the variable, it can be the positive literal or the
  // negative one. Only call this on an assigned variable.
  Literal GetTrueLiteralForAssignedVariable(VariableIndex var) const {
    DCHECK(IsVariableAssigned(var));
    return Literal(var, assignment_.IsSet(LiteralIndex(var.value() << 1)));
  }

  int NumberOfVariables() const { return assignment_.size().value() / 2; }

 private:
  // The encoding is as follow:
  // - assignment_.IsSet(literal.Index()) means literal is true.
  // - assignment_.IsSet(literal.Index() ^ 1]) means literal is false.
  // - If both are false, then the variable (and the literal) is unassigned.
  Bitset64<LiteralIndex> assignment_;

  DISALLOW_COPY_AND_ASSIGN(VariablesAssignment);
};

// A really simple wrapper used to pass a reference to a clause. This
// abstraction is needed because not all clauses come from an underlying
// SatClause or are encoded with a std::vector<Literal>.
//
// This class should be passed by value.
class ClauseRef {
 public:
  ClauseRef() : begin_(nullptr), end_(nullptr) {}
  ClauseRef(Literal const* b, Literal const* e) : begin_(b), end_(e) {}
  explicit ClauseRef(const std::vector<Literal>& literals)
      : begin_(&literals[0]), end_(&literals[0] + literals.size()) {}

  // Allows for range based iteration: for (Literal literal : clause_ref) {}.
  Literal const* begin() const { return begin_; }
  Literal const* end() const { return end_; }

  // Returns true if this clause contains no literal.
  bool IsEmpty() const { return begin_ == end_; }
  int size() const { return end_ - begin_; }

 private:
  Literal const* begin_;
  Literal const* end_;
};

// Forward declaration of the classes needed to compute the reason of an
// assignment.
class ResolutionNode;
class SatClause;
class UpperBoundedLinearConstraint;

// Information about a variable assignment.
struct AssignmentInfo {
  AssignmentInfo() {}

  // The type of assignment (this impact the reason for this assignment).
  //
  // Note(user): Another design for lazily evaluating the reason behind an
  // assignement could rely on virtual functions. Each constraint class can
  // subclass an HasReason class and implements a ComputeReason() virtual
  // function. This AssignmentInfo can then hold a pointer to an HasReason
  // class. Currently, this is not done this way for efficiency.
  enum Type {
    UNIT_REASON,
    SEARCH_DECISION,
    CLAUSE_PROPAGATION,
    BINARY_PROPAGATION,
    PB_PROPAGATION,
    SYMMETRY_PROPAGATION,
    SAME_REASON_AS,
    CACHED_REASON,
  };
  Type type;

  // The decision level at which this assignment was made. This starts at 0 and
  // increases each time the solver takes a SEARCH_DECISION.
  int level;

  // The index of this assignment in the trail.
  int trail_index;

  // The rest of this struct contains data about this assignment used to compute
  // the reason clause when it becomes needed. Note that depending on the type,
  // some fields will not be used and left uninitialized. We use unions to gain
  // a bit of memory.

// Visual C++ has a problem with a Literal inside an union.
#if defined(_MSC_VER)
  struct {
#else
  union {
#endif
    Literal literal;
    int source_trail_index;
  };

  union {
    SatClause* sat_clause;
    ResolutionNode* resolution_node;
    UpperBoundedLinearConstraint* pb_constraint;
    int symmetry_index;
    };
    VariableIndex reference_var;
};

// Note that we use <= because on 32 bits architecture, the size will actually
// be smaller than 24 bytes.
COMPILE_ASSERT(sizeof(AssignmentInfo) <= 24,
               ERROR_AssignmentInfo_is_not_well_compacted);

// The solver trail stores the assignement made by the solver in order.
// This class is responsible for maintaining the assignment of each variable
// and the information of each assignment.
class Trail {
 public:
  Trail() : num_enqueues_(0), trail_index_(0), need_level_zero_(false) {
    current_info_.level = 0;
  }

  void Resize(int num_variables) {
    assignment_.Resize(num_variables);
    info_.resize(num_variables);
    trail_.resize(num_variables);
    cached_reasons_.resize(num_variables);
    old_type_.resize(num_variables);
  }

  // Enqueues the assignment that make the given literal true on the trail. This
  // should only be called on unassigned variable. Extra information about this
  // assignment is controled by the Set*() functions below.
  void Enqueue(Literal true_literal, AssignmentInfo::Type type) {
    DCHECK(!assignment_.IsVariableAssigned(true_literal.Variable()));
    trail_[trail_index_] = true_literal;
    current_info_.trail_index = trail_index_;
    current_info_.type = type;
    info_[true_literal.Variable()] = current_info_;
    assignment_.AssignFromTrueLiteral(true_literal);
    ++num_enqueues_;
    ++trail_index_;
  }

  // Specific Enqueue() version for our different constraint types.
  void EnqueueWithUnitReason(Literal true_literal, ResolutionNode* node) {
    current_info_.resolution_node = node;
    Enqueue(true_literal, AssignmentInfo::UNIT_REASON);
  }
  void EnqueueWithBinaryReason(Literal true_literal, Literal reason) {
    current_info_.literal = reason;
    Enqueue(true_literal, AssignmentInfo::BINARY_PROPAGATION);
  }
  void EnqueueWithSatClauseReason(Literal true_literal, SatClause* clause) {
    current_info_.sat_clause = clause;
    Enqueue(true_literal, AssignmentInfo::CLAUSE_PROPAGATION);
  }
  void EnqueueWithPbReason(Literal true_literal, int source_trail_index,
                           UpperBoundedLinearConstraint* cst) {
    current_info_.source_trail_index = source_trail_index;
    current_info_.pb_constraint = cst;
    Enqueue(true_literal, AssignmentInfo::PB_PROPAGATION);
  }
  void EnqueueWithSymmetricReason(Literal true_literal, int source_trail_index,
                                  int symmetry_index) {
    current_info_.source_trail_index = source_trail_index;
    current_info_.symmetry_index = symmetry_index;
    Enqueue(true_literal, AssignmentInfo::SYMMETRY_PROPAGATION);
  }

  // Some constraints propagate a lot of literals at once. In these cases, it is
  // more efficient to have all the propagated literals except the first one
  // refering to the reason of the first of them.
  void EnqueueWithSameReasonAs(Literal true_literal,
                               VariableIndex reference_var) {
    current_info_.reference_var = reference_var;
    Enqueue(true_literal, AssignmentInfo::SAME_REASON_AS);
  }

  // Changes the type of the variable assignment to CACHED_REASON so that we
  // know that if it is needed again the reason can just be retrieved by
  // CachedReason(). Note that the returned vector needs to be filled with the
  // reason. If needed, you can get access to the initial assignment type with
  // InitialAssignmentType().
  //
  // Note(user): Changing the type is not "clean" but it is efficient. The idea
  // is that it is important to do as little as possible when pushing/poping
  // literal on the trail. Computing the reason happens a lot less often, so it
  // is okay to do slightly more work then. Note also, that we don't need to
  // do anything on "untrail", the CACHED_REASON type will be overwritten when
  // the same variable is assigned again.
  std::vector<Literal>* CacheReasonAtReturnedAddress(VariableIndex var) {
    old_type_[var] = info_[var].type;
    info_[var].type = AssignmentInfo::CACHED_REASON;
    return &(cached_reasons_[var]);
  }

  // Returns the reason for an assignment whose reason was cached.
  ClauseRef CachedReason(VariableIndex var) const {
    DCHECK_EQ(info_[var].type, AssignmentInfo::CACHED_REASON);
    return ClauseRef(cached_reasons_[var]);
  }

  // Returns the initial type of an assignment. This is basically the type
  // except for an assignment whose reason has now marked as cached where the
  // old type is returned.
  AssignmentInfo::Type InitialAssignmentType(VariableIndex var) const {
    const AssignmentInfo::Type type = info_[var].type;
    return type != AssignmentInfo::CACHED_REASON ? type : old_type_[var];
  }

  // Dequeues the last assigned literal and returns it.
  // Note that we do not touch its assignement info.
  Literal Dequeue() {
    --trail_index_;
    assignment_.UnassignLiteral(trail_[trail_index_]);
    return trail_[trail_index_];
  }

  // Changes the decision level used by the next Enqueue().
  void SetDecisionLevel(int level) { current_info_.level = level; }
  int CurrentDecisionLevel() const { return current_info_.level; }

  // Functions to store a failing clause.
  // There is a special version for a SatClause, because in this case we need to
  // be able to update its activity later.
  void SetFailingSatClause(ClauseRef ref, SatClause* clause) {
    failing_clause_ = ref;
    failing_sat_clause_ = clause;
  }
  void SetFailingClause(ClauseRef ref) {
    failing_clause_ = ref;
    failing_sat_clause_ = nullptr;
  }
  void SetFailingResolutionNode(ResolutionNode* node) { failing_node_ = node; }
  ClauseRef FailingClause() const { return failing_clause_; }
  SatClause* FailingSatClause() const { return failing_sat_clause_; }
  ResolutionNode* FailingResolutionNode() const { return failing_node_; }

  // This is required for producing correct unsat proof. Recall that a fixed
  // literals is one assigned at level zero. The option is here so every code
  // that needs it can easily access it.
  bool NeedFixedLiteralsInReason() const { return need_level_zero_; }
  void SetNeedFixedLiteralsInReason(bool value) { need_level_zero_ = value; }

  // Getters.
  int64 NumberOfEnqueues() const { return num_enqueues_; }
  int Index() const { return trail_index_; }
  const Literal operator[](int index) const { return trail_[index]; }
  const VariablesAssignment& Assignment() const { return assignment_; }
  const AssignmentInfo& Info(VariableIndex var) const {
    DCHECK_GE(var, 0);
    DCHECK_LT(var, info_.size());
    return info_[var];
  }

  // Sets the new resolution node for a variable that is fixed.
  void SetFixedVariableInfo(VariableIndex var, ResolutionNode* node) {
    CHECK_EQ(info_[var].level, 0);
    info_[var].type = AssignmentInfo::UNIT_REASON;
    info_[var].resolution_node = node;
  }

  // Print the current literals on the trail.
  std::string DebugString() {
    std::string result;
    for (int i = 0; i < trail_index_; ++i) {
      if (!result.empty()) result += " ";
      result += trail_[i].DebugString();
    }
    return result;
  }

 private:
  int64 num_enqueues_;
  int trail_index_;
  AssignmentInfo current_info_;
  VariablesAssignment assignment_;
  std::vector<Literal> trail_;
  ITIVector<VariableIndex, AssignmentInfo> info_;
  ClauseRef failing_clause_;
  SatClause* failing_sat_clause_;
  ResolutionNode* failing_node_;
  bool need_level_zero_;

  // Reason cache.
  ITIVector<VariableIndex, std::vector<Literal>> cached_reasons_;
  ITIVector<VariableIndex, AssignmentInfo::Type> old_type_;

  DISALLOW_COPY_AND_ASSIGN(Trail);
};

}  // namespace sat
}  // namespace operations_research

#endif  // OR_TOOLS_SAT_SAT_BASE_H_
