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

#include "ortools/sat/sat_base.h"

#include <string>
#include <utility>

#include "absl/log/check.h"
#include "absl/types/span.h"

namespace operations_research {
namespace sat {

// static
SatClause* SatClause::Create(absl::Span<const Literal> literals) {
  DCHECK_GE(literals.size(), 2);
  return CreateInternal(literals);
}

SatClause* SatClause::CreateInternal(absl::Span<const Literal> literals) {
  SatClause* clause = reinterpret_cast<SatClause*>(
      ::operator new(sizeof(SatClause) + literals.size() * sizeof(Literal)));
  clause->size_ = literals.size();
  for (int i = 0; i < literals.size(); ++i) {
    clause->literals_[i] = literals[i];
  }
  return clause;
}

// Note that for an attached clause, removing fixed literal is okay because if
// any of the watched literal is assigned, then the clause is necessarily true.
bool SatClause::RemoveFixedLiteralsAndTestIfTrue(
    const VariablesAssignment& assignment) {
  DCHECK(!IsRemoved());
  if (assignment.VariableIsAssigned(literals_[0].Variable()) ||
      assignment.VariableIsAssigned(literals_[1].Variable())) {
    DCHECK(IsSatisfied(assignment));
    return true;
  }
  int j = 2;
  while (j < size_ && !assignment.VariableIsAssigned(literals_[j].Variable())) {
    ++j;
  }
  for (int i = j; i < size_; ++i) {
    if (assignment.VariableIsAssigned(literals_[i].Variable())) {
      if (assignment.LiteralIsTrue(literals_[i])) return true;
    } else {
      std::swap(literals_[j], literals_[i]);
      ++j;
    }
  }
  size_ = j;
  return false;
}

bool SatClause::IsSatisfied(const VariablesAssignment& assignment) const {
  for (const Literal literal : *this) {
    if (assignment.LiteralIsTrue(literal)) return true;
  }
  return false;
}

std::string SatClause::DebugString() const {
  std::string result;
  for (const Literal literal : *this) {
    if (!result.empty()) result.append(" ");
    result.append(literal.DebugString());
  }
  return result;
}

}  // namespace sat
}  // namespace operations_research
