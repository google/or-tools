// Copyright 2010-2018 Google LLC
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

// Common types and constants used by the Linear Programming solver.

#ifndef OR_TOOLS_LP_DATA_LP_TYPES_H_
#define OR_TOOLS_LP_DATA_LP_TYPES_H_

#include <cmath>
#include <limits>

#include "ortools/base/basictypes.h"
#include "ortools/base/int_type.h"
#include "ortools/base/logging.h"
#include "ortools/base/strong_vector.h"
#include "ortools/util/bitset.h"

// We use typedefs as much as possible to later permit the usage of
// types such as quad-doubles or rationals.

namespace operations_research {
namespace glop {

// This type is defined to avoid cast issues during index conversions,
// e.g. converting ColIndex into RowIndex.
// All types should use 'Index' instead of int32.
typedef int32 Index;

// ColIndex is the type for integers representing column/variable indices.
// int32s are enough for handling even the largest problems.
DEFINE_INT_TYPE(ColIndex, Index);

// RowIndex is the type for integers representing row/constraint indices.
// int32s are enough for handling even the largest problems.
DEFINE_INT_TYPE(RowIndex, Index);

// Get the ColIndex corresponding to the column # row.
inline ColIndex RowToColIndex(RowIndex row) { return ColIndex(row.value()); }

// Get the RowIndex corresponding to the row # col.
inline RowIndex ColToRowIndex(ColIndex col) { return RowIndex(col.value()); }

// Get the integer index corresponding to the col.
inline Index ColToIntIndex(ColIndex col) { return col.value(); }

// Get the integer index corresponding to the row.
inline Index RowToIntIndex(RowIndex row) { return row.value(); }

// EntryIndex is the type for integers representing entry indices.
// An entry in a sparse matrix is a pair (row, value) for a given known column.
// See classes SparseColumn and SparseMatrix.
#if defined(__ANDROID__)
DEFINE_INT_TYPE(EntryIndex, int32);
#else
DEFINE_INT_TYPE(EntryIndex, int64);
#endif

static inline double ToDouble(double f) { return f; }

static inline double ToDouble(long double f) { return static_cast<double>(f); }

// The type Fractional denotes the type of numbers on which the computations are
// performed. This is defined as double here, but it could as well be float,
// DoubleDouble, QuadDouble, or infinite-precision rationals.
// Floating-point representations are binary fractional numbers, thus the name.
// (See http://en.wikipedia.org/wiki/Fraction_(mathematics) .)
typedef double Fractional;

// Range max for type Fractional. DBL_MAX for double for example.
const double kRangeMax = std::numeric_limits<double>::max();

// Infinity for type Fractional.
const double kInfinity = std::numeric_limits<double>::infinity();

// Epsilon for type Fractional, i.e. the smallest e such that 1.0 + e != 1.0 .
const double kEpsilon = std::numeric_limits<double>::epsilon();

// Returns true if the given value is finite, that means for a double:
// not a NaN and not +/- infinity.
inline bool IsFinite(Fractional value) {
  return value > -kInfinity && value < kInfinity;
}

// Constants to represent invalid row or column index.
// It is important that their values be the same because during transposition,
// one needs to be converted into the other.
const RowIndex kInvalidRow(-1);
const ColIndex kInvalidCol(-1);

// Different statuses for a given problem.
enum class ProblemStatus : int8 {
  // The problem has been solved to optimality. Both the primal and dual have
  // a feasible solution.
  OPTIMAL,

  // The problem has been proven primal-infeasible. Note that the problem is not
  // necessarily DUAL_UNBOUNDED (See Chvatal p.60). The solver does not have a
  // dual unbounded ray in this case.
  PRIMAL_INFEASIBLE,

  // The problem has been proven dual-infeasible. Note that the problem is not
  // necessarily PRIMAL_UNBOUNDED (See Chvatal p.60). The solver does
  // note have a primal unbounded ray in this case,
  DUAL_INFEASIBLE,

  // The problem is either INFEASIBLE or UNBOUNDED (this applies to both the
  // primal and dual algorithms). This status is only returned by the presolve
  // step and means that a primal or dual unbounded ray was found during
  // presolve. Note that because some presolve techniques assume that a feasible
  // solution exists to simplify the problem further, it is difficult to
  // distinguish between infeasibility and unboundedness.
  //
  // If a client needs to distinguish, it is possible to run the primal
  // algorithm on the same problem with a 0 objective function to know if the
  // problem was PRIMAL_INFEASIBLE.
  INFEASIBLE_OR_UNBOUNDED,

  // The problem has been proven feasible and unbounded. That means that the
  // problem is DUAL_INFEASIBLE and that the solver has a primal unbounded ray.
  PRIMAL_UNBOUNDED,

  // The problem has been proven dual-feasible and dual-unbounded. That means
  // the problem is PRIMAL_INFEASIBLE and that the solver has a dual unbounded
  // ray to prove it.
  DUAL_UNBOUNDED,

  // All the statuses below correspond to a case where the solver was
  // interrupted. This can happen because of a timeout, an iteration limit or an
  // error.

  // The solver didn't had a chance to prove anything.
  INIT,

  // The problem has been proven primal-feasible but may still be
  // PRIMAL_UNBOUNDED.
  PRIMAL_FEASIBLE,

  // The problem has been proven dual-feasible, but may still be DUAL_UNBOUNDED.
  // That means that if the primal is feasible, then it has a finite optimal
  // solution.
  DUAL_FEASIBLE,

  // An error occurred during the solving process.
  ABNORMAL,

  // The input problem was invalid (see LinearProgram.IsValid()).
  INVALID_PROBLEM,

  // The problem was solved to a feasible status, but the solution checker found
  // the primal and/or dual infeasibilities too important for the specified
  // parameters.
  IMPRECISE,
};

// Returns the string representation of the ProblemStatus enum.
std::string GetProblemStatusString(ProblemStatus problem_status);

inline std::ostream& operator<<(std::ostream& os, ProblemStatus status) {
  os << GetProblemStatusString(status);
  return os;
}

// Different types of variables.
enum class VariableType : int8 {
  UNCONSTRAINED,
  LOWER_BOUNDED,
  UPPER_BOUNDED,
  UPPER_AND_LOWER_BOUNDED,
  FIXED_VARIABLE
};

// Returns the string representation of the VariableType enum.
std::string GetVariableTypeString(VariableType variable_type);

inline std::ostream& operator<<(std::ostream& os, VariableType type) {
  os << GetVariableTypeString(type);
  return os;
}

// Different variables statuses.
// If a solution is OPTIMAL or FEASIBLE, then all the properties described here
// should be satisfied. These properties should also be true during the
// execution of the revised simplex algorithm, except that because of
// bound-shifting, the variable may not be at their exact bounds until the
// shifts are removed.
enum class VariableStatus : int8 {
  // The basic status is special and takes precedence over all the other
  // statuses. It means that the variable is part of the basis.
  BASIC,
  // Only possible status of a FIXED_VARIABLE not in the basis. The variable
  // value should be exactly equal to its bounds (which are the same).
  FIXED_VALUE,
  // Only possible statuses of a non-basic variable which is not UNCONSTRAINED
  // or FIXED. The variable value should be at its exact specified bound (which
  // must be finite).
  AT_LOWER_BOUND,
  AT_UPPER_BOUND,
  // Only possible status of an UNCONSTRAINED non-basic variable.
  // Its value should be zero.
  FREE,
};

// Returns the string representation of the VariableStatus enum.
std::string GetVariableStatusString(VariableStatus status);

inline std::ostream& operator<<(std::ostream& os, VariableStatus status) {
  os << GetVariableStatusString(status);
  return os;
}

// Different constraints statuses.
// The meaning is the same for the constraint activity relative to its bounds as
// it is for a variable value relative to its bounds. Actually, this is the
// VariableStatus of the slack variable associated to a constraint modulo a
// change of sign. The difference is that because of precision error, a
// constraint activity cannot exactly be equal to one of its bounds or to zero.
enum class ConstraintStatus : int8 {
  BASIC,
  FIXED_VALUE,
  AT_LOWER_BOUND,
  AT_UPPER_BOUND,
  FREE,
};

// Returns the string representation of the ConstraintStatus enum.
std::string GetConstraintStatusString(ConstraintStatus status);

inline std::ostream& operator<<(std::ostream& os, ConstraintStatus status) {
  os << GetConstraintStatusString(status);
  return os;
}

// Returns the ConstraintStatus corresponding to a given VariableStatus.
ConstraintStatus VariableToConstraintStatus(VariableStatus status);

// Wrapper around an ITIVector to allow (and enforce) creation/resize/assign
// to use the index type for the size.
//
// TODO(user): This should probably move into ITIVector, but note that this
// version is more strict and does not allow any other size types.
template <typename IntType, typename T>
class StrictITIVector : public absl::StrongVector<IntType, T> {
 public:
  typedef IntType IndexType;  // g++ 4.8.1 needs this.
  typedef absl::StrongVector<IntType, T> ParentType;
// This allows for brace initialization, which is really useful in tests.
// It is not 'explicit' by design, so one can do vector = {...};
#if !defined(__ANDROID__) && (!defined(_MSC_VER) || (_MSC_VER >= 1800))
  StrictITIVector(std::initializer_list<T> init_list)  // NOLINT
      : ParentType(init_list.begin(), init_list.end()) {}
#endif
  StrictITIVector() : ParentType() {}
  explicit StrictITIVector(IntType size) : ParentType(size.value()) {}
  StrictITIVector(IntType size, const T& v) : ParentType(size.value(), v) {}
  template <typename InputIteratorType>
  StrictITIVector(InputIteratorType first, InputIteratorType last)
      : ParentType(first, last) {}

  void resize(IntType size) { ParentType::resize(size.value()); }
  void resize(IntType size, const T& v) { ParentType::resize(size.value(), v); }

  void reserve(IntType size) { ParentType::reserve(size.value()); }

  void assign(IntType size, const T& v) { ParentType::assign(size.value(), v); }

  IntType size() const { return IntType(ParentType::size()); }

  IntType capacity() const { return IntType(ParentType::capacity()); }

  // Since calls to resize() must use a default value, we introduce a new
  // function for convenience to reduce the size of a vector.
  void resize_down(IntType size) {
    DCHECK_GE(size, IntType(0));
    DCHECK_LE(size, IntType(ParentType::size()));
    ParentType::resize(size.value());
  }

  // This function can be up to 4 times faster than calling assign(size, 0).
  // Note that it only works with StrictITIVector of basic types.
  void AssignToZero(IntType size) {
    resize(size, 0);
    memset(ParentType::data(), 0, size.value() * sizeof(T));
  }
};

// Row-vector types. Row-vector types are indexed by a column index.

// Row of fractional values.
typedef StrictITIVector<ColIndex, Fractional> DenseRow;

// Row of booleans.
typedef StrictITIVector<ColIndex, bool> DenseBooleanRow;

// Row of column indices. Used to represent mappings between columns.
typedef StrictITIVector<ColIndex, ColIndex> ColMapping;

// Vector of row or column indices. Useful to list the non-zero positions.
typedef std::vector<ColIndex> ColIndexVector;
typedef std::vector<RowIndex> RowIndexVector;

// Row of row indices.
// Useful for knowing which row corresponds to a particular column in the basis,
// or for storing the number of rows for a given column.
typedef StrictITIVector<ColIndex, RowIndex> ColToRowMapping;

// Row of variable types.
typedef StrictITIVector<ColIndex, VariableType> VariableTypeRow;

// Row of variable statuses.
typedef StrictITIVector<ColIndex, VariableStatus> VariableStatusRow;

// Row of bits.
typedef Bitset64<ColIndex> DenseBitRow;

// Column-vector types. Column-vector types are indexed by a row index.

// Column of fractional values.
typedef StrictITIVector<RowIndex, Fractional> DenseColumn;

// Column of booleans.
typedef StrictITIVector<RowIndex, bool> DenseBooleanColumn;

// Column of bits.
typedef Bitset64<RowIndex> DenseBitColumn;

// Column of row indices. Used to represent mappings between rows.
typedef StrictITIVector<RowIndex, RowIndex> RowMapping;

// Column of column indices.
// Used to represent which column corresponds to a particular row in the basis,
// or for storing the number of columns for a given row.
typedef StrictITIVector<RowIndex, ColIndex> RowToColMapping;

// Column of constraints (slack variables) statuses.
typedef StrictITIVector<RowIndex, ConstraintStatus> ConstraintStatusColumn;

// --------------------------------------------------------
// VectorIterator
// --------------------------------------------------------

// An iterator over the elements of a sparse data structure that stores the
// elements in arrays for indices and coefficients. The iterator is
// built as a wrapper over a sparse vector entry class; the concrete entry class
// is provided through the template argument EntryType.
template <typename EntryType>
class VectorIterator : EntryType {
 public:
  using Index = typename EntryType::Index;
  using Entry = EntryType;

  VectorIterator(const Index* indices, const Fractional* coefficients,
                 EntryIndex i)
      : EntryType(indices, coefficients, i) {}

  void operator++() { ++this->i_; }
  bool operator!=(const VectorIterator& other) const {
    // This operator is intended for use in natural range iteration ONLY.
    // Therefore, we prefer to use '<' so that a buggy range iteration which
    // start point is *after* its end point stops immediately, instead of
    // iterating 2^(number of bits of EntryIndex) times.
    return this->i_ < other.i_;
  }
  const Entry& operator*() const { return *this; }
};

// This is used during the deterministic time computation to convert a given
// number of floating-point operations to something in the same order of
// magnitude as a second (on a 2014 desktop).
static inline double DeterministicTimeForFpOperations(int64 n) {
  const double kConversionFactor = 2e-9;
  return kConversionFactor * static_cast<double>(n);
}

}  // namespace glop
}  // namespace operations_research

#endif  // OR_TOOLS_LP_DATA_LP_TYPES_H_
