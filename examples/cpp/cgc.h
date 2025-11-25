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

// Two-Dimensional Constrained Guillotine Cutting
//
// This file contains a solver for the Two-Dimensional Constrained
// Guillotine Cutting Problem. The problem requires cutting a plane
// rectangle into smaller rectangular pieces of given sizes and values
// in order to maximize the sum of the values of the cut pieces in which
// there is a constraint on the maximum number of each type of piece that
// is to be produced and all cuts go from one edge of the rectangle to be
// cut to the opposite edge.
//
// If cgc_time_limit_in_ms is defined, it provides the best value
// achieved in that amount of time.
//
// Example usage:
//
//    std::unique_ptr<operations_research::ConstrainedGuillotineCuttingData>
//    data(new operations_research::ConstrainedGuillotineCuttingData());
//    data->LoadFromFile(file_path);
//    operations_research::ConstrainedGuillotineCutting cgc(std::move(data));
//    cgc.Solve(absl::Milliseconds(absl::GetFlag(FLAGS_time_limit_in_ms)));
//    if (cgc.Solved()) {
//      cgc.PrintSolution();
//    }

#ifndef ORTOOLS_EXAMPLES_CGC_H_
#define ORTOOLS_EXAMPLES_CGC_H_

#include <algorithm>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "examples/cpp/cgc_data.h"
#include "ortools/constraint_solver/constraint_solver.h"

namespace operations_research {

class ConstrainedGuillotineCutting {
 public:
  struct CutRectangle {
    CutRectangle(int parent_index, int length, int width)
        : parent_index(parent_index), length(length), width(width) {}

    int parent_index;
    int length;
    int width;
  };

  explicit ConstrainedGuillotineCutting(
      std::unique_ptr<ConstrainedGuillotineCuttingData> data)
      : data_(std::move(data)),
        solver_("ConstrainedGuillotineCutting"),
        solved_(false),
        maximum_value_(0) {}

  int MaximumValue() const {
    DCHECK(solved_);
    return maximum_value_;
  }
  bool Solved() const { return solved_; }

  void PrintSolution() const;
  void Solve(absl::Duration time_limit);

 private:
  // Contains the problem parameters.
  std::unique_ptr<ConstrainedGuillotineCuttingData> data_;
  Solver solver_;

  bool solved_;
  int maximum_value_;
  std::vector<CutRectangle> solution_;
};

}  // namespace operations_research

#endif  // ORTOOLS_EXAMPLES_CGC_H_
