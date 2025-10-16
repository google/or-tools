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

#ifndef OR_TOOLS_SAT_INTEGER_RESOLUTION_H_
#define OR_TOOLS_SAT_INTEGER_RESOLUTION_H_

#include <cstdint>
#include <limits>
#include <string>
#include <vector>

#include "absl/types/span.h"
#include "ortools/base/strong_vector.h"
#include "ortools/sat/clause.h"
#include "ortools/sat/integer.h"
#include "ortools/sat/integer_base.h"
#include "ortools/sat/model.h"
#include "ortools/sat/sat_base.h"
#include "ortools/sat/sat_solver.h"
#include "ortools/sat/synchronization.h"
#include "ortools/util/bitset.h"

namespace operations_research::sat {

// Conflict resolution at the "integer level" a bit like if all our integer
// literals where already instantiated as boolean.
//
// In addition we can minimize the conflict by exploiting the relationship
// between integer literal on the same variable, like x >= 5  =>  x >= 3.
//
// Depending on the options, this code might generate new Boolean during
// conflict resolution, or keep expanding the integer literals until we only
// have Booleans left.
class IntegerConflictResolution {
 public:
  explicit IntegerConflictResolution(Model* model);
  ~IntegerConflictResolution();

  // Same interface as the SAT based one.
  //
  // TODO(user): Support LRAT proof, at least for pure Boolean problems.
  void ComputeFirstUIPConflict(
      std::vector<Literal>* conflict,
      std::vector<Literal>* reason_used_to_infer_the_conflict,
      std::vector<SatClause*>* subsumed_clauses);

 private:
  // Returns the list of integer_literals associated with an index.
  absl::Span<const IntegerLiteral> IndexToIntegerLiterals(
      GlobalTrailIndex index);

  // Remove from subsumed_clauses the one that are not subsumed.
  // It is a bit hard to track down cardinality during our various optim, so
  // this is easier to make sure we are correct. I rescan the subsumed_clauses
  // candidates a second time, which isn't too bad.
  void FilterSubsumedClauses(std::vector<Literal>* conflict,
                             std::vector<SatClause*>* subsumed_clauses);

  // Adds to our processing queue the reason for source_index.
  // This is also called for the initial conflict, with a dummy source_index.
  void AddToQueue(GlobalTrailIndex source_index, const IntegerReason& reason,
                  std::vector<SatClause*>* subsumed_clauses);

  // Updates int_data_[i_lit.var] and add an entry to the queue if needed.
  void ProcessIntegerLiteral(GlobalTrailIndex source_index,
                             IntegerLiteral i_lit);

  // If a variable has holes and one need to explain var >= value, if the value
  // fall into a hole of the domain, we actually only need var >= smaller_value.
  // This returns that smaller value.
  IntegerValue RelaxBoundIfHoles(IntegerVariable var, IntegerValue value);

  // Debugging function to print info about a GlobalTrailIndex.
  std::string DebugGlobalIndex(GlobalTrailIndex index);
  std::string DebugGlobalIndex(absl::Span<const GlobalTrailIndex> indices);

  Trail* trail_;
  IntegerTrail* integer_trail_;
  IntegerEncoder* integer_encoder_;
  SatSolver* sat_solver_;
  SharedStatistics* shared_stats_;
  ClauseManager* clauses_propagator_;
  BinaryImplicationGraph* implications_;
  const SatParameters& params_;

  // A heap. We manage it manually.
  mutable std::vector<GlobalTrailIndex> tmp_queue_;

  // Information about the current content of our tmp_queue_ and our conflict
  // resolution.
  SparseBitset<int> tmp_bool_index_seen_;
  SparseBitset<BooleanVariable> tmp_bool_seen_;
  std::vector<IntegerLiteral> tmp_integer_literals_;
  util_intops::StrongVector<IntegerVariable, IntegerValue>
      tmp_var_to_settled_lb_;

  // The current occurrence of this integer variable in the reason.
  struct IntegerVariableData {
    // Whether this variable was added in the queue.
    // If false, index_in_queue will be the index to re-add it with.
    bool in_queue = false;
    int int_index_in_queue = std::numeric_limits<int>::max();

    // We only need var >= bound in the current conflict resolution.
    // Note that we have: integer_trail_[int_index_in_queue] >= bound.
    IntegerValue bound = kMinIntegerValue;
  };
  util_intops::StrongVector<IntegerVariable, IntegerVariableData> int_data_;

  // Stats.
  int64_t num_conflicts_at_wrong_level_ = 0;
  int64_t num_expansions_ = 0;
  int64_t num_subsumed_ = 0;
  int64_t num_conflict_literals_ = 0;
  int64_t num_associated_integer_for_literals_in_conflict_ = 0;
  int64_t num_associated_literal_use_ = 0;
  int64_t num_associated_literal_fail_ = 0;
  int64_t num_possibly_non_optimal_reason_ = 0;
  int64_t num_slack_usage_ = 0;
  int64_t num_slack_relax_ = 0;
  int64_t num_holes_relax_ = 0;
  int64_t num_created_1uip_bool_ = 0;
  int64_t num_binary_minimization_ = 0;

  // Stats to compare with old conflict resolution.
  int64_t comparison_num_win_ = 0;
  int64_t comparison_num_same_ = 0;
  int64_t comparison_num_loose_ = 0;
  int64_t comparison_old_sum_of_literals_ = 0;
  int64_t comparison_old_num_subsumed_ = 0;
};

}  // namespace operations_research::sat

#endif  // OR_TOOLS_SAT_INTEGER_RESOLUTION_H_
