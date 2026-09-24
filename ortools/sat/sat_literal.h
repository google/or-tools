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

// Sat Literal (a boolean variable or its negation).

#ifndef ORTOOLS_SAT_LITERAL_H_
#define ORTOOLS_SAT_LITERAL_H_

#include <algorithm>
#include <ostream>
#include <string>
#include <utility>
#include <vector>

#include "absl/log/check.h"
#include "absl/log/log.h"
#include "absl/strings/str_format.h"
#include "absl/types/span.h"
#include "ortools/util/strong_integers.h"

namespace operations_research {
namespace sat {

// Index of a variable (>= 0).
DEFINE_STRONG_INDEX_TYPE(BooleanVariable);
const BooleanVariable kNoBooleanVariable(-1);

// Index of a literal (>= 0), see Literal below.
DEFINE_STRONG_INDEX_TYPE(LiteralIndex);
const LiteralIndex kNoLiteralIndex(-1);

// Special values used in some API to indicate a literal that is always true
// or always false.
const LiteralIndex kTrueLiteralIndex(-2);
const LiteralIndex kFalseLiteralIndex(-3);

// A literal is used to represent a variable or its negation. If it represents
// the variable it is said to be positive. If it represents its negation, it is
// said to be negative. We support two representations as an integer.
//
// The "signed" encoding of a literal is convenient for input/output and is used
// in the cnf file format. For a 0-based variable index x, (x + 1) represents
// the variable x and -(x + 1) represents its negation. The signed value 0 is an
// undefined literal and this class can never contain it.
//
// The "index" encoding of a literal is convenient as an index to an array
// and is the one used internally for efficiency. It is always positive or zero,
// and for a 0-based variable index x, (x << 1) encodes the variable x and the
// same number XOR 1 encodes its negation.
class Literal {
 public:
  explicit constexpr Literal(int signed_value)
      : index_(signed_value > 0 ? ((signed_value - 1) << 1)
                                : ((-signed_value - 1) << 1) ^ 1) {
    CHECK_NE(signed_value, 0);
  }

  Literal() = default;
  explicit constexpr Literal(LiteralIndex index) : index_(index.value()) {}
  Literal(BooleanVariable variable, bool is_positive)
      : index_(is_positive ? (variable.value() << 1)
                           : (variable.value() << 1) ^ 1) {}

  // We want a literal to be implicitly converted to a LiteralIndex().
  // Before this, we used to have many literal.Index() that didn't add anything.
  //
  // TODO(user): LiteralIndex might not even be needed, but because of the
  // signed value business, it is still safer with it.
  operator LiteralIndex() const { return Index(); }  // NOLINT

  BooleanVariable Variable() const { return BooleanVariable(index_ >> 1); }
  bool IsPositive() const { return !(index_ & 1); }
  bool IsNegative() const { return (index_ & 1); }

  LiteralIndex Index() const { return LiteralIndex(index_); }
  LiteralIndex NegatedIndex() const { return LiteralIndex(index_ ^ 1); }

  int SignedValue() const {
    return (index_ & 1) ? -((index_ >> 1) + 1) : ((index_ >> 1) + 1);
  }

  Literal Negated() const { return Literal(NegatedIndex()); }

  std::string DebugString() const {
    if (index_ == kNoLiteralIndex.value()) return "NA";
    return absl::StrFormat("%+d", SignedValue());
  }

  bool operator==(Literal other) const { return index_ == other.index_; }
  bool operator!=(Literal other) const { return index_ != other.index_; }
  bool operator<(const Literal& other) const { return index_ < other.index_; }
  bool operator<=(const Literal& other) const { return index_ <= other.index_; }

  template <typename H>
  friend H AbslHashValue(H h, Literal literal) {
    return H::combine(std::move(h), literal.index_);
  }

 private:
  int index_;
};

inline std::ostream& operator<<(std::ostream& os, Literal literal) {
  os << literal.DebugString();
  return os;
}

template <typename Sink, typename... T>
void AbslStringify(Sink& sink, Literal arg) {
  absl::Format(&sink, "%s", arg.DebugString());
}

inline std::ostream& operator<<(std::ostream& os,
                                absl::Span<const Literal> literals) {
  os << "[";
  bool first = true;
  for (const Literal literal : literals) {
    if (first) {
      first = false;
    } else {
      os << ",";
    }
    os << literal.DebugString();
  }
  os << "]";
  return os;
}

inline std::ostream& operator<<(std::ostream& os,
                                absl::Span<const LiteralIndex> literals) {
  os << "[";
  bool first = true;
  for (const LiteralIndex index : literals) {
    if (first) {
      first = false;
    } else {
      os << ",";
    }
    os << Literal(index).DebugString();
  }
  os << "]";
  return os;
}

// Only used for testing to use the classical SAT notation for a literal. This
// allows to write Literals({+1, -4, +3}) for the clause with BooleanVariable 0
// and 2 appearing positively and 3 negatively.
inline std::vector<Literal> Literals(absl::Span<const int> input) {
  std::vector<Literal> result(input.size());
  for (int i = 0; i < result.size(); ++i) {
    result[i] = Literal(input[i]);
  }
  return result;
}

}  // namespace sat
}  // namespace operations_research

#endif  // ORTOOLS_SAT_LITERAL_H_
