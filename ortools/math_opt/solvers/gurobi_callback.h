// Copyright 2010-2022 Google LLC
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

#ifndef OR_TOOLS_MATH_OPT_SOLVERS_GUROBI_CALLBACK_H_
#define OR_TOOLS_MATH_OPT_SOLVERS_GUROBI_CALLBACK_H_

#include <cstdint>
#include <vector>

#include "absl/container/flat_hash_set.h"
#include "absl/status/status.h"
#include "absl/time/time.h"
#include "ortools/base/linked_hash_map.h"
#include "ortools/gurobi/environment.h"
#include "ortools/math_opt/callback.pb.h"
#include "ortools/math_opt/core/solve_interrupter.h"
#include "ortools/math_opt/core/solver_interface.h"
#include "ortools/math_opt/solvers/gurobi/g_gurobi.h"
#include "ortools/math_opt/solvers/message_callback_data.h"
#include "ortools/math_opt/sparse_containers.pb.h"

namespace operations_research {
namespace math_opt {

struct GurobiCallbackInput {
  SolverInterface::Callback user_cb;
  SolverInterface::MessageCallback message_cb;
  const gtl::linked_hash_map<int64_t, int>& variable_ids;
  int num_gurobi_vars = 0;
  // events[i] indicates if we should run user_cb when Gurobi's callback is
  // triggered with where=i. See:
  //   https://www.gurobi.com/documentation/9.1/refman/cb_codes.html
  // For list of possible values for where. See also EventToGurobiWhere() below.
  std::vector<bool> events;
  const SparseVectorFilterProto& mip_solution_filter;
  const SparseVectorFilterProto& mip_node_filter;
  const absl::Time start;
};

// Converts a set of CallbackEventProto enums to a bit vector indicating which
// Gurobi callback events need to run our callback.
//
// The returned vector will have an entry for each possible value of "where"
// that Gurobi's callbacks can stop at, see the table here:
//   https://www.gurobi.com/documentation/9.1/refman/cb_codes.html
std::vector<bool> EventToGurobiWhere(
    const absl::flat_hash_set<CallbackEventProto>& events);

absl::Status GurobiCallbackImpl(const Gurobi::CallbackContext& context,
                                const GurobiCallbackInput& callback_input,
                                MessageCallbackData& message_callback_data,
                                SolveInterrupter* local_interrupter);

// Makes the final calls to the message callback with any unfinished line if
// necessary. It must be called once at the end of the solve, even when the
// solve or one callback failed (in case the last unfinished line contains some
// details about that).
void GurobiCallbackImplFlush(const GurobiCallbackInput& callback_input,
                             MessageCallbackData& message_callback_data);

}  // namespace math_opt
}  // namespace operations_research
#endif  // OR_TOOLS_MATH_OPT_SOLVERS_GUROBI_CALLBACK_H_
