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

#include "ortools/lp_data/lp_utils.h"

#include <algorithm>

#include "absl/log/check.h"
#include "absl/types/span.h"
#include "ortools/lp_data/lp_types.h"
#include "ortools/lp_data/scattered_vector.h"
#include "ortools/lp_data/sparse_column.h"

namespace operations_research {
namespace glop {

template <typename SparseColumnLike>
Fractional SquaredNormTemplate(const SparseColumnLike& column) {
  Fractional sum(0.0);
  for (const SparseColumn::Entry e : column) {
    sum += Square(e.coefficient());
  }
  return sum;
}

Fractional SquaredNorm(const SparseColumn& v) {
  return SquaredNormTemplate<SparseColumn>(v);
}

Fractional SquaredNorm(const ColumnView& v) {
  return SquaredNormTemplate<ColumnView>(v);
}

Fractional PreciseSquaredNorm(const SparseColumn& v) {
  KahanSum sum;
  for (const SparseColumn::Entry e : v) {
    sum.Add(Square(e.coefficient()));
  }
  return sum.Value();
}

Fractional SquaredNorm(const ScatteredColumn& v) {
  if (v.ShouldUseDenseIteration()) {
    return SquaredNorm(v.values);
  }
  Fractional sum(0.0);
  for (const RowIndex row : v.non_zeros) {
    sum += Square(v[row]);
  }
  return sum;
}

Fractional PreciseSquaredNorm(const ScatteredColumn& v) {
  if (v.ShouldUseDenseIteration()) {
    return PreciseSquaredNorm(v.values);
  }
  KahanSum sum;
  for (const RowIndex row : v.non_zeros) {
    sum.Add(Square(v[row]));
  }
  return sum.Value();
}

Fractional SquaredNorm(absl::Span<const Fractional> data) {
  // We expand ourselves since we don't really care about the floating
  // point order of operation and this seems faster.
  int i = 0;
  const int end = data.size();
  const int shifted_end = end - 3;
  Fractional sum1 = 0.0;
  Fractional sum2 = 0.0;
  Fractional sum3 = 0.0;
  Fractional sum4 = 0.0;
  for (; i < shifted_end; i += 4) {
    sum1 += data[i] * data[i];
    sum2 += data[i + 1] * data[i + 1];
    sum3 += data[i + 2] * data[i + 2];
    sum4 += data[i + 3] * data[i + 3];
  }
  Fractional sum = sum1 + sum2 + sum3 + sum4;
  if (i < end) {
    sum += data[i] * data[i];
    if (i + 1 < end) {
      sum += data[i + 1] * data[i + 1];
      if (i + 2 < end) {
        sum += data[i + 2] * data[i + 2];
      }
    }
  }
  return sum;
}

Fractional SquaredNormAndResetToZero(absl::Span<Fractional> data) {
  // We expand ourselves since we don't really care about the floating
  // point order of operation and this seems faster.
  int i = 0;
  const int end = data.size();
  const int shifted_end = end - 3;
  Fractional sum1 = 0.0;
  Fractional sum2 = 0.0;
  Fractional sum3 = 0.0;
  Fractional sum4 = 0.0;
  for (; i < shifted_end; i += 4) {
    sum1 += data[i] * data[i];
    sum2 += data[i + 1] * data[i + 1];
    sum3 += data[i + 2] * data[i + 2];
    sum4 += data[i + 3] * data[i + 3];
    data[i] = 0.0;
    data[i + 1] = 0.0;
    data[i + 2] = 0.0;
    data[i + 3] = 0.0;
  }
  Fractional sum = sum1 + sum2 + sum3 + sum4;
  if (i < end) {
    sum += data[i] * data[i];
    data[i] = 0.0;
    if (i + 1 < end) {
      sum += data[i + 1] * data[i + 1];
      data[i + 1] = 0.0;
      if (i + 2 < end) {
        sum += data[i + 2] * data[i + 2];
        data[i + 2] = 0.0;
      }
    }
  }
  return sum;
}

Fractional SquaredNorm(const DenseColumn& column) {
  return SquaredNorm(absl::MakeSpan(column.data(), column.size().value()));
}

Fractional PreciseSquaredNorm(const DenseColumn& column) {
  KahanSum sum;
  for (RowIndex row(0); row < column.size(); ++row) {
    sum.Add(Square(column[row]));
  }
  return sum.Value();
}

Fractional InfinityNorm(const DenseColumn& v) {
  Fractional infinity_norm = 0.0;
  for (RowIndex row(0); row < v.size(); ++row) {
    infinity_norm = std::max(infinity_norm, fabs(v[row]));
  }
  return infinity_norm;
}

template <typename SparseColumnLike>
Fractional InfinityNormTemplate(const SparseColumnLike& column) {
  Fractional infinity_norm = 0.0;
  for (const SparseColumn::Entry e : column) {
    infinity_norm = std::max(infinity_norm, fabs(e.coefficient()));
  }
  return infinity_norm;
}

Fractional InfinityNorm(const SparseColumn& v) {
  return InfinityNormTemplate<SparseColumn>(v);
}

Fractional InfinityNorm(const ColumnView& v) {
  return InfinityNormTemplate<ColumnView>(v);
}

double Density(const DenseRow& row) {
  if (row.empty()) return 0.0;
  int sum = 0.0;
  for (ColIndex col(0); col < row.size(); ++col) {
    if (row[col] != Fractional(0.0)) ++sum;
  }
  return static_cast<double>(sum) / row.size().value();
}

void RemoveNearZeroEntries(Fractional threshold, DenseRow* row) {
  if (threshold == Fractional(0.0)) return;
  for (ColIndex col(0); col < row->size(); ++col) {
    if (fabs((*row)[col]) < threshold) {
      (*row)[col] = Fractional(0.0);
    }
  }
}

void RemoveNearZeroEntries(Fractional threshold, DenseColumn* column) {
  if (threshold == Fractional(0.0)) return;
  for (RowIndex row(0); row < column->size(); ++row) {
    if (fabs((*column)[row]) < threshold) {
      (*column)[row] = Fractional(0.0);
    }
  }
}

Fractional RestrictedInfinityNorm(const ColumnView& column,
                                  const DenseBooleanColumn& rows_to_consider,
                                  RowIndex* row_index) {
  Fractional infinity_norm = 0.0;
  for (const SparseColumn::Entry e : column) {
    if (rows_to_consider[e.row()] && fabs(e.coefficient()) > infinity_norm) {
      infinity_norm = fabs(e.coefficient());
      *row_index = e.row();
    }
  }
  return infinity_norm;
}

void SetSupportToFalse(const ColumnView& column, DenseBooleanColumn* b) {
  for (const SparseColumn::Entry e : column) {
    if (e.coefficient() != 0.0) {
      (*b)[e.row()] = false;
    }
  }
}

bool IsDominated(const ColumnView& column, const DenseColumn& radius) {
  for (const SparseColumn::Entry e : column) {
    DCHECK_GE(radius[e.row()], 0.0);
    if (fabs(e.coefficient()) > radius[e.row()]) return false;
  }
  return true;
}

}  // namespace glop
}  // namespace operations_research
