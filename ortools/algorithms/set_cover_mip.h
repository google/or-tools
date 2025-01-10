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

#ifndef OR_TOOLS_ALGORITHMS_SET_COVER_MIP_H_
#define OR_TOOLS_ALGORITHMS_SET_COVER_MIP_H_

#include "absl/types/span.h"
#include "ortools/algorithms/set_cover_invariant.h"
#include "ortools/algorithms/set_cover_model.h"

namespace operations_research {
enum class SetCoverMipSolver : int {
  SCIP = 0,
  SAT = 1,
  GUROBI = 2,
  GLOP = 3,
  PDLP = 4
};

class SetCoverMip {
 public:
  // Simpler constructor that uses SCIP by default.
  explicit SetCoverMip(SetCoverInvariant* inv)
      : inv_(inv), mip_solver_(SetCoverMipSolver::SCIP) {}

  // The constructor takes a SetCoverInvariant that will store the resulting
  // variable choices, and a MIP Solver.
  SetCoverMip(SetCoverInvariant* inv, SetCoverMipSolver mip_solver)
      : inv_(inv), mip_solver_(mip_solver) {}

  // Returns true if a solution was found.
  // If use_integers is false, lower_bound_ is populated with a linear
  // lower bound.
  // time_limit_in_seconds is a (rather soft) time limit for the execution time.
  // TODO(user): Add time-outs and exit with a partial solution. This seems
  // unlikely, though.
  bool NextSolution(bool use_integers, double time_limit_in_seconds);

  // Computes the next partial solution considering only the subsets whose
  // indices are in focus.
  bool NextSolution(absl::Span<const SubsetIndex> focus, bool use_integers,
                    double time_limit_in_seconds);

  // Returns the lower bound of the linear relaxation of the problem.
  double lower_bound() const { return lower_bound_; }

 private:
  // The invariant used to maintain the state of the problem.
  SetCoverInvariant* inv_;

  // The MIP solver flavor used by the instance.
  SetCoverMipSolver mip_solver_;

  // The lower bound of the problem, when use_integers is false. The MIP with
  // continuous variables becomes a computationally simpler linear program.
  double lower_bound_;
};
}  // namespace operations_research

#endif  // OR_TOOLS_ALGORITHMS_SET_COVER_MIP_H_
