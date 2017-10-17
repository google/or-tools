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

#include "ortools/lp_data/matrix_utils.h"
#include <algorithm>
#include "ortools/base/hash.h"

namespace operations_research {
namespace glop {

namespace {

// Returns true iff the two given sparse columns are proportional. The two
// sparse columns must be ordered by row and must not contain any zero entry.
//
// See the header comment on FindProportionalColumns() for the exact definition
// of two proportional columns with a given tolerance.
bool AreColumnsProportional(const SparseColumn& a, const SparseColumn& b,
                            Fractional tolerance) {
  DCHECK(a.IsCleanedUp());
  DCHECK(b.IsCleanedUp());
  if (a.num_entries() != b.num_entries()) return false;
  Fractional multiple = 0.0;
  bool a_is_larger = true;
  for (const EntryIndex i : a.AllEntryIndices()) {
    if (a.EntryRow(i) != b.EntryRow(i)) return false;
    const Fractional coeff_a = a.EntryCoefficient(i);
    const Fractional coeff_b = b.EntryCoefficient(i);
    if (multiple == 0.0) {
      a_is_larger = fabs(coeff_a) > fabs(coeff_b);
      multiple = a_is_larger ? coeff_a / coeff_b : coeff_b / coeff_a;
    } else {
      if (a_is_larger) {
        if (fabs(coeff_a / coeff_b - multiple) > tolerance) return false;
      } else {
        if (fabs(coeff_b / coeff_a - multiple) > tolerance) return false;
      }
    }
  }
  return true;
}

// A column index together with its fingerprint. See ComputeFingerprint().
struct ColumnFingerprint {
  ColumnFingerprint(ColIndex _col, int32 _hash, double _value)
      : col(_col), hash(_hash), value(_value) {}
  ColIndex col;
  int32 hash;
  double value;

  // This order has the property that if AreProportionalCandidates() is true for
  // two given columns, then in a sorted list of columns
  // AreProportionalCandidates() will be true for all the pairs of columns
  // between the two given ones (included).
  bool operator<(const ColumnFingerprint& other) const {
    if (hash == other.hash) {
      return value < other.value;
    }
    return hash < other.hash;
  }
};

// Two columns can be proportional only if:
// - Their non-zero pattern hashes are the same.
// - Their double fingerprints are close to each other.
bool AreProportionalCandidates(ColumnFingerprint a, ColumnFingerprint b,
                               Fractional tolerance) {
  if (a.hash != b.hash) return false;
  return fabs(a.value - b.value) < tolerance;
}

// The fingerprint of a column has two parts:
// - A hash value of the column non-zero pattern.
// - A double value which should be the same for two proportional columns
//   modulo numerical errors.
ColumnFingerprint ComputeFingerprint(ColIndex col, const SparseColumn& column) {
  int32 non_zero_pattern_hash = 0;
  Fractional min_abs = std::numeric_limits<Fractional>::max();
  Fractional max_abs = 0.0;
  Fractional sum = 0.0;
  for (const SparseColumn::Entry e : column) {
    non_zero_pattern_hash =
        Hash32NumWithSeed(e.row().value(), non_zero_pattern_hash);
    sum += e.coefficient();
    min_abs = std::min(min_abs, fabs(e.coefficient()));
    max_abs = std::max(max_abs, fabs(e.coefficient()));
  }

  // The two scaled values are in [0, 1].
  // TODO(user): A better way to discriminate columns would be to take the
  // scalar product with a constant but random vector scaled by max_abs.
  DCHECK_NE(0.0, max_abs);
  const double inverse_dynamic_range = min_abs / max_abs;
  const double scaled_average =
      fabs(sum) / (static_cast<double>(column.num_entries().value()) * max_abs);
  return ColumnFingerprint(col, non_zero_pattern_hash,
                           inverse_dynamic_range + scaled_average);
}

}  // namespace

ColMapping FindProportionalColumns(const SparseMatrix& matrix,
                                   Fractional tolerance) {
  const ColIndex num_cols = matrix.num_cols();
  ColMapping mapping(num_cols, kInvalidCol);

  // Compute the fingerprint of each columns and sort them.
  std::vector<ColumnFingerprint> fingerprints;
  for (ColIndex col(0); col < num_cols; ++col) {
    if (!matrix.column(col).IsEmpty()) {
      fingerprints.push_back(ComputeFingerprint(col, matrix.column(col)));
    }
  }
  std::sort(fingerprints.begin(), fingerprints.end());

  // Find a representative of each proportional columns class. This only
  // compares columns with a close-enough fingerprint.
  for (int i = 0; i < fingerprints.size(); ++i) {
    const ColIndex col_a = fingerprints[i].col;
    if (mapping[col_a] != kInvalidCol) continue;
    for (int j = i + 1; j < fingerprints.size(); ++j) {
      const ColIndex col_b = fingerprints[j].col;
      if (mapping[col_b] != kInvalidCol) continue;

      // Note that we use the same tolerance for the fingerprints.
      // TODO(user): Derive precise bounds on what this tolerance should be so
      // that no proportional columns are missed.
      if (!AreProportionalCandidates(fingerprints[i], fingerprints[j],
                                     tolerance)) {
        break;
      }
      if (AreColumnsProportional(matrix.column(col_a), matrix.column(col_b),
                                 tolerance)) {
        mapping[col_b] = col_a;
      }
    }
  }

  // Sort the mapping so that the representative of each class is the smallest
  // column. To achieve this, the current representative is used as a pointer
  // to the new one, a bit like in an union find algorithm.
  for (ColIndex col(0); col < num_cols; ++col) {
    if (mapping[col] == kInvalidCol) continue;
    const ColIndex new_representative = mapping[mapping[col]];
    if (new_representative != kInvalidCol) {
      mapping[col] = new_representative;
    } else {
      if (mapping[col] > col) {
        mapping[mapping[col]] = col;
        mapping[col] = kInvalidCol;
      }
    }
  }

  return mapping;
}

ColMapping FindProportionalColumnsUsingSimpleAlgorithm(
    const SparseMatrix& matrix, Fractional tolerance) {
  const ColIndex num_cols = matrix.num_cols();
  ColMapping mapping(num_cols, kInvalidCol);
  for (ColIndex col_a(0); col_a < num_cols; ++col_a) {
    if (mapping[col_a] != kInvalidCol) continue;
    for (ColIndex col_b(col_a + 1); col_b < num_cols; ++col_b) {
      if (mapping[col_b] != kInvalidCol) continue;
      if (AreColumnsProportional(matrix.column(col_a), matrix.column(col_b),
                                 tolerance)) {
        mapping[col_b] = col_a;
      }
    }
  }
  return mapping;
}

bool AreFirstColumnsAndRowsExactlyEquals(RowIndex num_rows, ColIndex num_cols,
                                         const SparseMatrix& matrix_a,
                                         const CompactSparseMatrix& matrix_b) {
  // TODO(user): Also DCHECK() that matrix_b is ordered by rows.
  DCHECK(matrix_a.IsCleanedUp());
  if (num_rows > matrix_a.num_rows() || num_rows > matrix_b.num_rows() ||
      num_cols > matrix_a.num_cols() || num_cols > matrix_b.num_cols()) {
    return false;
  }
  for (ColIndex col(0); col < num_cols; ++col) {
    const SparseColumn& col_a = matrix_a.column(col);
    const CompactSparseMatrix::ColumnView& col_b = matrix_b.column(col);
    const EntryIndex end = std::min(col_a.num_entries(), col_b.num_entries());
    for (EntryIndex i(0); i < end; ++i) {
      if (col_a.EntryRow(i) != col_b.EntryRow(i)) {
        if (col_a.EntryRow(i) < num_rows || col_b.EntryRow(i) < num_rows) {
          return false;
        } else {
          break;
        }
      }
      if (col_a.EntryCoefficient(i) != col_b.EntryCoefficient(i)) {
        return false;
      }
      if (col_a.num_entries() > end && col_a.EntryRow(end) < num_rows) {
        return false;
      }
      if (col_b.num_entries() > end && col_b.EntryRow(end) < num_rows) {
        return false;
      }
    }
  }
  return true;
}

bool IsRightMostSquareMatrixIdentity(const SparseMatrix& matrix) {
  DCHECK(matrix.IsCleanedUp());
  if (matrix.num_rows().value() > matrix.num_cols().value()) return false;
  const ColIndex first_identity_col =
      matrix.num_cols() - RowToColIndex(matrix.num_rows());
  for (ColIndex col = first_identity_col; col < matrix.num_cols(); ++col) {
    const SparseColumn& column = matrix.column(col);
    if (column.num_entries() != 1 ||
        column.EntryCoefficient(EntryIndex(0)) != 1.0) {
      return false;
    }
  }
  return true;
}

}  // namespace glop
}  // namespace operations_research
