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

#ifndef ORTOOLS_SAT_INTEGER_RESOLUTION_H_
#define ORTOOLS_SAT_INTEGER_RESOLUTION_H_

#include <cstdint>
#include <limits>
#include <string>
#include <vector>

#include "absl/types/span.h"
#include "ortools/base/strong_vector.h"
#include "ortools/base/types.h"
#include "ortools/sat/clause.h"
#include "ortools/sat/integer.h"
#include "ortools/sat/integer_base.h"
#include "ortools/sat/model.h"
#include "ortools/sat/sat_base.h"
#include "ortools/sat/sat_parameters.pb.h"
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
      std::vector<Literal>* reason_used_to_infer_the_conflict);

 private:
  // This is used for "linear slack" where we can relax a reason of the form
  // (var >= bound) to (var >= bound - delta) as long as delta * coeff <= slack.
  //
  // The actual slack is in linear_slacks_[slack_index] and is shared amongst
  // many SlackData.
  DEFINE_STRONG_INDEX_TYPE(SlackIndex);
  DEFINE_STRONG_INDEX_TYPE(SlackListIndex);
  constexpr static SlackListIndex kNoListIndex = SlackListIndex(-1);
  struct SlackData {
    IntegerValue coeff;
    SlackIndex slack_index;
    SlackListIndex next = kNoListIndex;
  };

  // The current occurrence of this integer variable in the reason.
  struct IntegerVariableData {
    // Whether this variable was added in the queue.
    // If false, index_in_queue will be the index to re-add it with.
    bool in_queue = false;
    int int_index_in_queue = kint32max;

    // We only need var >= bound in the current conflict resolution.
    // Note that we have: integer_trail_[int_index_in_queue] >= bound.
    //
    // Important: If slacks is non empty, we might actually require more, i.e.
    // var >= integer_trail_[int_index_in_queue] (with some slack).
    IntegerValue bound = kMinIntegerValue;

    // We already added to the reason a literal that prove var >= settled_bound.
    // So there is no need to prove any var >= rhs for rhs smaller than this.
    IntegerValue settled_bound = kMinIntegerValue;

    // If empty, we just need to explain var >= bound.
    //
    // Otherwise, we need var >= bound_at_index, but that bound can be relaxed
    // up to var >= bound by consuming the correct quantity from all the linear
    // slack listed here.
    //
    // Note that this is usually constructed once and accessed only twice. So a
    // linked list seems like a good datastructure for that. Note also that
    // we never reclaim the linked_list_buffer_ memory during a single
    // resolution.
    SlackListIndex slack_ptr = kNoListIndex;
  };

  // Clears the slack for the given variable, and update its bound.
  // There is no need to prove anything less tight than var >= threshold.
  void ConsumeSlack(IntegerVariable var,
                    IntegerValue threshold = kMinIntegerValue);

  // Returns the list of integer_literals associated with an index.
  absl::Span<const IntegerLiteral> IndexToIntegerLiterals(
      GlobalTrailIndex index);

  // Adds to our processing queue the reason for source_index.
  // This is also called for the initial conflict, with a dummy source_index.
  void ExpandAndAddReasonToQueue(GlobalTrailIndex source_index,
                                 const IntegerReason& reason,
                                 std::optional<IntegerValue> bound);

  // Updates int_data_[i_lit.var] and add an entry to the queue if needed.
  void ProcessIntegerLiteral(GlobalTrailIndex source_index,
                             IntegerLiteral i_lit);

  // If a variable has holes and one need to explain var >= value, if the value
  // fall into a hole of the domain, we actually only need var >= smaller_value.
  // This returns that smaller value.
  IntegerValue RelaxBoundIfHoles(IntegerVariable var, IntegerValue value);

  // Marks all integer literals associated to one of the given Boolean literals
  // as "no need to be expanded further".
  void MarkAllAssociatedLiterals(absl::Span<const Literal> literals);

  // Debugging function to print info about a GlobalTrailIndex.
  std::string DebugGlobalIndex(GlobalTrailIndex index);
  std::string DebugGlobalIndex(absl::Span<const GlobalTrailIndex> indices);

  // Updates the given IntegerVariableData.
  void UpdateData(IntegerVariable var, GlobalTrailIndex source_index,
                  IntegerVariableData& data);

  // Wrapper to access int_data_[var] with support for sparse clear.
  IntegerVariableData& MutableIntData(IntegerVariable var);

  // Add the integer variable entry to the queue.
  void AddToQueueIfNotThere(IntegerVariableData& data);

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
  std::vector<IntegerLiteral> tmp_integer_literals_;

  // Per IntegerVariable information.
  // IMPORTANT: This should only be accessed via MutableIntData() !!
  SparseBitset<IntegerVariable> touched_int_data_;
  util_intops::StrongVector<IntegerVariable, IntegerVariableData> int_data_;

  // For handling the slack of relaxed linear reasons.
  util_intops::StrongVector<SlackIndex, IntegerValue> linear_slacks_;
  util_intops::StrongVector<SlackListIndex, SlackData> linked_list_buffer_;

  // Stats.
  int64_t num_conflicts_at_wrong_level_ = 0;
  int64_t num_expansions_ = 0;
  int64_t num_conflict_literals_ = 0;
  int64_t num_associated_integer_for_literals_in_conflict_ = 0;
  int64_t num_associated_literal_use_ = 0;
  int64_t num_associated_literal_fail_ = 0;
  int64_t num_possibly_non_optimal_reason_ = 0;
  int64_t num_slack_usage_ = 0;
  int64_t num_slack_increase_ = 0;
  int64_t num_slack_relax_ = 0;
  int64_t num_holes_relax_ = 0;
  int64_t num_created_1uip_bool_ = 0;
  int64_t num_binary_minimization_ = 0;

  // Stats to compare with old conflict resolution.
  int64_t comparison_num_win_ = 0;
  int64_t comparison_num_same_ = 0;
  int64_t comparison_num_loose_ = 0;
  int64_t comparison_old_sum_of_literals_ = 0;
};

}  // namespace operations_research::sat

#endif  // ORTOOLS_SAT_INTEGER_RESOLUTION_H_
