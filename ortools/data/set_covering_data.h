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

#ifndef OR_TOOLS_DATA_SET_COVERING_DATA_H_
#define OR_TOOLS_DATA_SET_COVERING_DATA_H_

#include <vector>

#include "ortools/base/integral_types.h"

namespace operations_research {
namespace scp {

class ScpData {
 public:
  ScpData() : is_set_partitioning_(false) {}
  // Getters.
  int num_rows() const { return columns_per_row_.size(); }
  int num_columns() const { return rows_per_column_.size(); }
  // columns_per_row[i][j] returns the index of the jth column covering row i.
  const std::vector<std::vector<int>>& columns_per_row() const {
    return columns_per_row_;
  }
  // rows_per_column[i][j] returns the index of the jth row covering column i.
  const std::vector<std::vector<int>>& rows_per_column() const {
    return rows_per_column_;
  }
  const std::vector<int>& column_costs() const { return column_costs_; }

  bool is_set_partitioning() const { return is_set_partitioning_; }
  void set_is_set_partitioning(bool v) { is_set_partitioning_ = v; }

  // Builders.
  // Calling SetProblemSize() will clear all previous data.
  void SetProblemSize(int num_rows, int num_columns);
  void SetColumnCost(int column_id, int cost);
  void AddRowInColumn(int row, int column);

 private:
  std::vector<std::vector<int>> columns_per_row_;
  std::vector<std::vector<int>> rows_per_column_;
  std::vector<int> column_costs_;
  bool is_set_partitioning_;
};

}  // namespace scp
}  // namespace operations_research

#endif  // OR_TOOLS_DATA_SET_COVERING_DATA_H_
