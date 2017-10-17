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


#include "ortools/lp_data/lp_utils.h"

namespace operations_research {
namespace glop {

Fractional SquaredNorm(const SparseColumn& v) {
  Fractional sum(0.0);
  for (const SparseColumn::Entry e : v) {
    sum += Square(e.coefficient());
  }
  return sum;
}

Fractional PreciseSquaredNorm(const SparseColumn& v) {
  KahanSum sum;
  for (const SparseColumn::Entry e : v) {
    sum.Add(Square(e.coefficient()));
  }
  return sum.Value();
}

Fractional PreciseSquaredNorm(ScatteredColumnReference v) {
  if (v.non_zero_rows.size() >
      ScatteredColumnReference::kDenseThresholdForPreciseSum *
          v.dense_column.size().value()) {
    return PreciseSquaredNorm(v.dense_column);
  }
  KahanSum sum;
  for (const RowIndex row : v.non_zero_rows) {
    sum.Add(Square(v[row]));
  }
  return sum.Value();
}

Fractional SquaredNorm(const DenseColumn& column) {
  Fractional sum(0.0);
  for (RowIndex row(0); row < column.size(); ++row) {
    sum += Square(column[row]);
  }
  return sum;
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

Fractional InfinityNorm(const SparseColumn& v) {
  Fractional infinity_norm = 0.0;
  for (const SparseColumn::Entry e : v) {
    infinity_norm = std::max(infinity_norm, fabs(e.coefficient()));
  }
  return infinity_norm;
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

Fractional RestrictedInfinityNorm(const SparseColumn& column,
                                  const DenseBooleanColumn& row_to_consider,
                                  RowIndex* row_index) {
  Fractional infinity_norm = 0.0;
  for (const SparseColumn::Entry e : column) {
    if (row_to_consider[e.row()] && fabs(e.coefficient()) > infinity_norm) {
      infinity_norm = fabs(e.coefficient());
      *row_index = e.row();
    }
  }
  return infinity_norm;
}

void SetSupportToFalse(const SparseColumn& column, DenseBooleanColumn* b) {
  for (const SparseColumn::Entry e : column) {
    if (e.coefficient() != 0.0) {
      (*b)[e.row()] = false;
    }
  }
}

bool IsDominated(const SparseColumn& column, const DenseColumn& radius) {
  for (const SparseColumn::Entry e : column) {
    DCHECK_GE(radius[e.row()], 0.0);
    if (fabs(e.coefficient()) > radius[e.row()]) return false;
  }
  return true;
}

}  // namespace glop
}  // namespace operations_research
