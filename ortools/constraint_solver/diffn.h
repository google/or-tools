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

#ifndef ORTOOLS_CONSTRAINT_SOLVER_DIFFN_H_
#define ORTOOLS_CONSTRAINT_SOLVER_DIFFN_H_

#include <cstdint>
#include <string>
#include <vector>

#include "absl/container/flat_hash_set.h"
#include "absl/log/check.h"
#include "absl/types/span.h"
#include "ortools/constraint_solver/constraint_solver.h"

namespace operations_research {

class Diffn : public Constraint {
 public:
  Diffn(Solver* solver, const std::vector<IntVar*>& x_vars,
        const std::vector<IntVar*>& y_vars, const std::vector<IntVar*>& x_size,
        const std::vector<IntVar*>& y_size, bool strict)
      : Constraint(solver),
        x_(x_vars),
        y_(y_vars),
        dx_(x_size),
        dy_(y_size),
        strict_(strict),
        size_(x_vars.size()),
        fail_stamp_(0) {
    CHECK_EQ(x_vars.size(), y_vars.size());
    CHECK_EQ(x_vars.size(), x_size.size());
    CHECK_EQ(x_vars.size(), y_size.size());
  }

  ~Diffn() override {}

  void Post() override;

  void InitialPropagate() override;

  std::string DebugString() const override;

  void Accept(ModelVisitor* visitor) const override;

 private:
  void PropagateAll();

  void OnBoxRangeChange(int box);

  bool CanBoxedOverlap(int i, int j) const;

  bool AreBoxedDisjoingHorizontallyForSure(int i, int j) const;

  bool AreBoxedDisjoingVerticallyForSure(int i, int j) const;

  // Fill neighbors_ with all boxes that can overlap the given box.
  void FillNeighbors(int box);

  // Fails if the minimum area of the given box plus the area of its neighbors
  // (that must already be computed in neighbors_) is greater than the area of a
  // bounding box that necessarily contains all these boxes.
  void FailWhenEnergyIsTooLarge(int box);

  // Changes the domain of all the neighbors of a given box (that must
  // already be computed in neighbors_) so that they can't overlap the
  // mandatory part of the given box.
  void PushOverlappingBoxes(int box);

  // Changes the domain of the two given box by excluding the value that
  // make them overlap for sure. Note that this function is symmetric in
  // the sense that its argument can be swapped for the same result.
  void PushOneBox(int box, int other);

  Constraint* MakeCumulativeConstraint(const std::vector<IntVar*>& positions,
                                       absl::Span<const int64_t> sizes,
                                       const std::vector<IntVar*>& demands,
                                       int64_t capacity);

  std::vector<IntVar*> x_;
  std::vector<IntVar*> y_;
  std::vector<IntVar*> dx_;
  std::vector<IntVar*> dy_;
  const bool strict_;
  const int64_t size_;
  Demon* delayed_demon_;
  absl::flat_hash_set<int> to_propagate_;
  std::vector<int> neighbors_;
  uint64_t fail_stamp_;
};

}  // namespace operations_research

#endif  // ORTOOLS_CONSTRAINT_SOLVER_DIFFN_H_
