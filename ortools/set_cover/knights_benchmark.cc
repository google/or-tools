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

#include "benchmark/benchmark.h"
#include "ortools/set_cover/base_types.h"
#include "ortools/set_cover/set_cover_heuristics.h"
#include "ortools/set_cover/set_cover_invariant.h"
#include "ortools/set_cover/set_cover_model.h"

namespace operations_research {
namespace {

class KnightsCover {
 public:
  KnightsCover(int num_rows, int num_cols)
      : num_rows_(num_rows), num_cols_(num_cols), model_() {
    constexpr int knight_row_move[] = {2, 1, -1, -2, -2, -1, 1, 2};
    constexpr int knight_col_move[] = {1, 2, 2, 1, -1, -2, -2, -1};
    for (int row = 0; row < num_rows_; ++row) {
      for (int col = 0; col < num_cols_; ++col) {
        model_.AddEmptySubset(1);
        model_.AddElementToLastSubset(ElementNumber(row, col));
        for (int i = 0; i < 8; ++i) {
          const int new_row = row + knight_row_move[i];
          const int new_col = col + knight_col_move[i];
          if (IsOnBoard(new_row, new_col)) {
            model_.AddElementToLastSubset(ElementNumber(new_row, new_col));
          }
        }
      }
    }
  }

  SetCoverModel model() const { return model_; }

 private:
  bool IsOnBoard(int row, int col) const {
    return row >= 0 && row < num_rows_ && col >= 0 && col < num_cols_;
  }
  ElementIndex ElementNumber(int row, int col) const {
    return ElementIndex(row * num_cols_ + col);
  }
  int num_rows_;
  int num_cols_;
  SetCoverModel model_;
};

static constexpr int SIZE = 128;

void BM_Steepest(benchmark::State& state) {
  for (auto s : state) {
    SetCoverModel model = KnightsCover(SIZE, SIZE).model();
    SetCoverInvariant inv(&model);
    GreedySolutionOptimizer greedy(&inv);
    SteepestSearch steepest(&inv);
  }
}

BENCHMARK(BM_Steepest)->Arg(1 << 5);

}  // namespace
}  // namespace operations_research
