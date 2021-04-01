// Copyright 2010-2021 Google LLC
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

#include "ortools/data/set_covering_data.h"

namespace operations_research {
namespace scp {

void ScpData::SetProblemSize(int num_rows, int num_columns) {
  columns_per_row_.clear();
  columns_per_row_.resize(num_rows);
  rows_per_column_.clear();
  rows_per_column_.resize(num_columns);
  column_costs_.resize(num_columns, 0);
}

void ScpData::SetColumnCost(int column_id, int cost) {
  column_costs_[column_id] = cost;
}

void ScpData::AddRowInColumn(int row_id, int column_id) {
  rows_per_column_[column_id].push_back(row_id);
  columns_per_row_[row_id].push_back(column_id);
}

}  // namespace scp
}  // namespace operations_research
