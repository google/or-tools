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

// A sat clause is a disjunction of literals.

#ifndef ORTOOLS_SAT_SAT_CLAUSE_H_
#define ORTOOLS_SAT_SAT_CLAUSE_H_

#include <algorithm>
#include <array>
#include <cstdint>
#include <memory>
#include <string>
#include <utility>

#include "absl/base/casts.h"
#include "absl/log/check.h"
#include "absl/log/log.h"
#include "absl/strings/str_format.h"
#include "absl/types/span.h"
#include "ortools/sat/sat_assignment.h"
#include "ortools/sat/sat_literal.h"

namespace operations_research {
namespace sat {

// This is how the SatSolver stores a clause. A clause is just a disjunction of
// literals. In many places, we just use vector<literal> to encode one. But in
// the critical propagation code, we use this class to remove one memory
// indirection.
class SatClause {
 public:
  // Creates a sat clause. There must be at least 2 literals.
  // Clauses with one literal fix variables directly and are never constructed.
  // Note that in practice, we use BinaryImplicationGraph for clauses of size
  // 2, so this is used for size at least 3.
  static SatClause* Create(absl::Span<const Literal> literals);

  // Non-sized delete because this is a tail-padded class.
  void operator delete(void* p) { delete[] static_cast<char*>(p); }

  // Number of literals in the clause.
  int size() const { return size_; }
  bool empty() const { return size_ == 0; }

  // We re-use the size to lazily remove clauses and notify that they need to be
  // deleted. It is why this is not called empty() to emphasize that fact. Note
  // that we never create an initially empty clause, so there is no confusion
  // with an infeasible model with an empty clause inside.
  bool IsRemoved() const { return size_ == 0; }

  // Allows for range based iteration: for (Literal literal : clause) {}.
  const Literal* begin() const { return &(literals_[0]); }
  const Literal* end() const { return &(literals_[size_]); }

  // Returns the first and second literals. These are always the watched
  // literals if the clause is attached in the LiteralWatchers.
  Literal FirstLiteral() const { return literals_[0]; }
  Literal SecondLiteral() const { return literals_[1]; }

  // Returns the literal that was propagated to true. This only works for a
  // clause that just propagated this literal. Otherwise, this will just return
  // a literal of the clause.
  Literal PropagatedLiteral() const { return literals_[0]; }

  // Returns the reason for the last unit propagation of this clause. The
  // preconditions are the same as for PropagatedLiteral(). Note that we don't
  // need to include the propagated literal.
  absl::Span<const Literal> PropagationReason() const {
    return absl::Span<const Literal>(&(literals_[1]), size_ - 1);
  }

  // Returns a Span<> representation of the clause.
  absl::Span<const Literal> AsSpan() const {
    return absl::Span<const Literal>(&(literals_[0]), size_);
  }

  // Returns true if the clause is satisfied for the given assignment. Note that
  // the assignment may be partial, so false does not mean that the clause can't
  // be satisfied by completing the assignment.
  bool IsSatisfied(const VariablesAssignment& assignment) const;

  std::string DebugString() const;

 private:
  // The manager needs to permute the order of literals in the clause and
  // call MarkForRemoval()/Rewrite. ClausePtr needs CreateInternal() to be able
  // to create instances with fewer than two literals.
  friend class ClauseManager;
  friend class ClausePtr;

  static SatClause* CreateInternal(absl::Span<const Literal> literals);

  explicit SatClause(absl::Span<const Literal> literals)
      : size_(literals.size()) {
    std::uninitialized_copy(literals.begin(), literals.end(), literals_);
  }

  Literal* literals() { return &(literals_[0]); }

  // Marks the clause so that the next call to CleanUpWatchers() can identify it
  // and actually remove it. After calling this the only valid operation is to
  // call `IsRemoved()`.
  void MarkForRemoval() const { size_ = 0; }

  // Removes literals that are fixed. This should only be called at level 0
  // where a literal is fixed iff it is assigned. Aborts and returns true if
  // they are not all false.
  //
  // Note that the removed literals can still be accessed in the portion [size,
  // old_size) of literals().
  bool RemoveFixedLiteralsAndTestIfTrue(const VariablesAssignment& assignment);

  // Rewrites a clause with another shorter one. Note that the clause shouldn't
  // be attached when this is called.
  void Rewrite(absl::Span<const Literal> new_clause) {
    size_ = 0;
    for (const Literal l : new_clause) literals_[size_++] = l;
  }

  mutable int32_t size_;  // Mutable for `MarkForRemoval()`.

  // This class stores the literals inline, and literals_ marks the start of the
  // variable length portion.
  Literal literals_[0];
};

// A clause pointer. This is either a SatClause pointer or, for clauses with at
// most 2 literals, the literals themselves. At any given time two distinct
// active (i.e., created and not yet deleted) clauses are guaranteed to have
// different pointers. On the other hand, several pointers can describe the same
// set of literals. Also, a given pointer can describe different clauses at
// different times (in such cases it is important to delete the first clause
// before reusing its pointer for a new one).
class ClausePtr {
 public:
  enum Type {
    kEmptyClause,
    kUnitClause,
    kBinaryClause,
    kSatClause,
  };

  // Returns `kNullClausePtr`.
  constexpr ClausePtr() : ClausePtr({LiteralIndex(0), LiteralIndex(0)}) {}

  // Returns the pointer to the empty clause.
  static constexpr ClausePtr EmptyClausePtr() {
    return ClausePtr({LiteralIndex(0), LiteralIndex(kEmptyClauseBits)});
  }

  // Creates the pointer to the given unit clause.
  explicit ClausePtr(Literal a)
      : ClausePtr({a.Index(), LiteralIndex(kUnitClauseBit)}) {}

  // Creates the pointer to the given binary clause. The two literals must be
  // different. The result does not depend on their order.
  ClausePtr(Literal a, Literal b)
      : ClausePtr(std::minmax(a.Index(), b.Index())) {
    DCHECK_NE(a, b);
  }

  // Creates a ClausePtr from a SatClause pointer.
  explicit ClausePtr(const SatClause* clause) {
    // Make sure we can store SatClause pointers without losing information.
    static_assert(sizeof(uint64_t) >= sizeof(uintptr_t));
    static_assert(alignof(SatClause) >= 2);
    const uintptr_t ptr_rep = absl::bit_cast<uintptr_t>(clause);
    const uint64_t bits = kSatClauseBit | (static_cast<uint64_t>(ptr_rep) >> 1);
    rep_ = rep_from_uint64(bits);
  }

  // Creates a SatClause with the given literals and returns its pointer. This
  // always creates a `kSatClause` ClausePtr, even if there are 2 literals or
  // less.
  explicit ClausePtr(absl::Span<const Literal> literals)
      : ClausePtr(SatClause::CreateInternal(literals)) {}

  // Returns the type of this pointer, which must not be null.
  Type GetType() const {
    DCHECK_NE(*this, ClausePtr());
    // Switch on bits (b2,b1,b0) = (rep_[1]_31, rep_[0]_31, rep_[0]_30):
    // - 011 : empty clause
    // - 010 : unit clause
    // - 00* : binary clause
    // - 1** : sat clause
    // Compiles down to simple code with clang: https://godbolt.org/z/3d7czGxaT.
    const uint64_t bits = uint64_from_rep(rep_);
    switch (((bits >> 61) & 4) | ((bits >> 30) & 3)) {
      case 0b000:
      case 0b001:
        return kBinaryClause;
      case 0b010:
        return kUnitClause;
      case 0b011:
        return kEmptyClause;
      default:
        return kSatClause;
    }
  }

  // Returns true if this pointer is a `kBinaryClause` pointer.
  bool IsBinaryClausePtr() const {
    const uint64_t bits = uint64_from_rep(rep_);
    const uint64_t kBinaryClauseBits = 0x80000000'80000000;
    return bits != 0 && (bits & kBinaryClauseBits) == 0;
  }

  // Returns the first literal of the pointer's target clause. The pointer must
  // not be null and must be a `kUnitClause` or `kBinaryClause` pointer. For
  // binary clauses, the literal order is unspecified.
  Literal GetFirstLiteral() const {
    DCHECK_NE(*this, ClausePtr());
    DCHECK(GetType() == kUnitClause || GetType() == kBinaryClause);
    return rep_[1];
  }

  // Returns the second literal of the pointer's target clause. The pointer must
  // not be null and must be a `kBinaryClause` pointer. The literal order is
  // unspecified.
  Literal GetSecondLiteral() const {
    DCHECK_NE(*this, ClausePtr());
    DCHECK_EQ(GetType(), kBinaryClause);
    return rep_[0];
  }

  // Returns the literals of the pointer's target clause. The pointer must not
  // be null.
  absl::Span<const Literal> GetLiterals() const {
    DCHECK_NE(*this, ClausePtr());
    switch (GetType()) {
      case kEmptyClause:
        return absl::Span<const Literal>();
      case kUnitClause:
        return absl::Span<const Literal>(rep_.data() + 1, 1);
      case kBinaryClause:
        return absl::Span<const Literal>(rep_.data(), 2);
      default:
        return GetSatClause()->AsSpan();
    }
  }

  // Returns true if this pointer is a `kSatClause` pointer.
  bool IsSatClausePtr() const {
    return (rep_[1].Index().value() & (kSatClauseBit >> 32)) != 0;
  }

  // Returns the SatClause pointer corresponding to this pointer.
  // IsSatClausePtr() must be true.
  SatClause* GetSatClause() const {
    DCHECK(IsSatClausePtr());
    // This is fine even if uintptr_t is smaller than uint64_t because, in this
    // case, the extra bits are zero (see how SatClause* IDs are created).
    const uint64_t bits = uint64_from_rep(rep_);
    const uintptr_t ptr_rep = static_cast<uintptr_t>(bits << 1);
    return absl::bit_cast<SatClause*>(ptr_rep);
  }

  // Returns a uint64_t representation of this pointer (and not of its target
  // clause).
  uint64_t SerializePtr() const {
    // TODO(user): reorder the bits, depending on GetType() in a bijective
    // way, to get small values (in order to improve compression in protos,
    // which use a varint encoding). Also omit the ID in protos when it can be
    // recomputed from the literals.
    return uint64_from_rep(rep_);
  }

  bool operator==(ClausePtr other) const { return rep_ == other.rep_; }
  bool operator!=(ClausePtr other) const { return rep_ != other.rep_; }

  template <typename H>
  friend H AbslHashValue(H h, const ClausePtr& clause) {
    return H::combine(std::move(h), clause.rep_);
  }

  template <typename Sink>
  friend void AbslStringify(Sink& sink, const ClausePtr& clause) {
    absl::Format(&sink, "%lu", uint64_from_rep(clause.rep_));
  }

 private:
  static constexpr uint32_t kEmptyClauseBits = 0xC0000000;
  static constexpr uint32_t kUnitClauseBit = 0x80000000;
  static constexpr uint64_t kSatClauseBit = 0x80000000'00000000;

  // literals.first, literals.second must be rep_[1], rep_[0], respectively.
  constexpr explicit ClausePtr(std::pair<LiteralIndex, LiteralIndex> literals)
      : rep_({Literal(literals.second), Literal(literals.first)}) {}

  // Returns (rep_[1], rep_[0]) as an uint64_t. Compiles down to a single
  // instruction on most architectures (https://godbolt.org/z/KdjK3aev9).
  static uint64_t uint64_from_rep(std::array<Literal, 2> rep) {
    return (static_cast<uint64_t>(rep[1].Index().value()) << 32) |
           static_cast<uint64_t>(static_cast<uint32_t>(rep[0].Index().value()));
  }

  // Returns rep[1] = bits_63..32, rep[0] = bits_31..0. Compiles down to a
  // single instruction on most architectures (https://godbolt.org/z/KdjK3aev9).
  static std::array<Literal, 2> rep_from_uint64(uint64_t bits) {
    return {Literal(LiteralIndex(static_cast<uint32_t>(bits))),
            Literal(LiteralIndex(static_cast<uint32_t>(bits >> 32)))};
  }

  // The clause pointer, encoded as follows (the order of the array elements is
  // chosen so that SatClause* pointers can be bit_cast to this representation
  // on little endian platforms, the most common ones):
  //   rep[1] rep[0]
  // - 000... 000... : the null clause pointer.
  // - 000... 110... : the pointer of the empty clause.
  // - 0xxx.. 10.... : the pointer of a unit clause. The 31 xxx bits are the
  //                   literal index of the single literal of this clause.
  // - 0xxx.. 0yyy.. : the pointer of a binary clause. The 31 xxx (resp. yyy)
  //                   bits are the smallest (resp. largest) literal index of
  //                   the two literals of this clause. The two literals must be
  //                   different, hence a binary clause pointer cannot be
  //                   confused with the null clause pointer.
  // - 1xxx.. xxx... : a SatClause* pointer. The 63 xxx bits are the pointer
  //                   value shifted right by 1. Due to alignment the LSB of the
  //                   pointer should be 0.
  std::array<Literal, 2> rep_;
};

// The null clause pointer.
constexpr ClausePtr kNullClausePtr = ClausePtr();

// Creates a clause pointer for the given literals. If there are more than 2
// literals, allocates a SatClause and returns its pointer.
ClausePtr NewClausePtr(absl::Span<const Literal> literals);

}  // namespace sat
}  // namespace operations_research

#endif  // ORTOOLS_SAT_SAT_CLAUSE_H_
