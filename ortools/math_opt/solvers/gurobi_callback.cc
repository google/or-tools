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

#include "ortools/math_opt/solvers/gurobi_callback.h"

#include <cstdint>
#include <functional>
#include <limits>
#include <string>
#include <utility>
#include <vector>

#include "ortools/base/logging.h"
#include "absl/container/flat_hash_set.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "absl/time/clock.h"
#include "absl/time/time.h"
#include "absl/types/optional.h"
#include "absl/types/span.h"
#include "ortools/base/linked_hash_map.h"
#include "ortools/math_opt/callback.pb.h"
#include "ortools/math_opt/core/math_opt_proto_utils.h"
#include "ortools/math_opt/core/solver_interface.h"
#include "ortools/math_opt/core/sparse_vector_view.h"
#include "ortools/math_opt/solution.pb.h"
#include "ortools/math_opt/solvers/message_callback_data.h"
#include "ortools/math_opt/sparse_containers.pb.h"
#include "ortools/base/status_macros.h"
#include "ortools/base/protoutil.h"

#include "ortools/gurobi/environment.h"

namespace operations_research {
namespace math_opt {
namespace {

// The number of possible values for "where" that Gurobi's callbacks can stop
// at, see the table here:
//   https://www.gurobi.com/documentation/9.1/refman/cb_codes.html
constexpr int kNumGurobiEvents = 9;
constexpr int kGrbOk = 0;
constexpr double kInf = std::numeric_limits<double>::infinity();

template <int where>
constexpr int CheckedGuroibWhere() {
  static_assert(where >= 0 && where < kNumGurobiEvents);
  return where;
}

inline int GurobiEvent(CallbackEventProto event) {
  switch (event) {
    case CALLBACK_EVENT_POLLING:
      return CheckedGuroibWhere<GRB_CB_POLLING>();
    case CALLBACK_EVENT_PRESOLVE:
      return CheckedGuroibWhere<GRB_CB_PRESOLVE>();
    case CALLBACK_EVENT_SIMPLEX:
      return CheckedGuroibWhere<GRB_CB_SIMPLEX>();
    case CALLBACK_EVENT_MIP:
      return CheckedGuroibWhere<GRB_CB_MIP>();
    case CALLBACK_EVENT_MIP_SOLUTION:
      return CheckedGuroibWhere<GRB_CB_MIPSOL>();
    case CALLBACK_EVENT_MIP_NODE:
      return CheckedGuroibWhere<GRB_CB_MIPNODE>();
    case CALLBACK_EVENT_BARRIER:
      return CheckedGuroibWhere<GRB_CB_BARRIER>();
    case CALLBACK_EVENT_MESSAGE:
      return CheckedGuroibWhere<GRB_CB_MESSAGE>();
    case CALLBACK_EVENT_UNSPECIFIED:
    default:
      LOG(FATAL) << "Unexpected callback event: " << event;
  }
}

absl::Status GurobiStatus(GRBmodel* model, int error_code) {
  if (error_code == kGrbOk) {
    return absl::OkStatus();
  }
  GRBenv* const env = GRBgetenv(model);
  return absl::InternalError(
      absl::StrCat("Gurobi error ", error_code, ": ", GRBgeterrormsg(env)));
}

SparseDoubleVectorProto ApplyFilter(
    const std::vector<double>& grb_solution,
    const gtl::linked_hash_map<int64_t, int>& var_ids,
    const SparseVectorFilterProto& filter) {
  SparseVectorFilterPredicate predicate(filter);
  SparseDoubleVectorProto result;
  for (const auto [id, grb_index] : var_ids) {
    const double val = grb_solution[grb_index];
    if (predicate.AcceptsAndUpdate(id, val)) {
      result.add_ids(id);
      result.add_values(val);
    }
  }
  return result;
}

class GurobiCallbackContext {
 public:
  GurobiCallbackContext(GRBmodel* model, void* cbdata, int where)
      : model_(model), cbdata_(cbdata), where_(where) {}

  absl::StatusOr<int> get_int(int what) const {
    int result;
    RETURN_IF_ERROR(AsStatus(GRBcbget(cbdata_, where_, what, &result)));
    return result;
  }

  absl::StatusOr<double> get_double(int what) const {
    double result;
    RETURN_IF_ERROR(AsStatus(GRBcbget(cbdata_, where_, what, &result)));
    return result;
  }

  absl::StatusOr<int64_t> get_int64(int what) const {
    double result;
    RETURN_IF_ERROR(AsStatus(GRBcbget(cbdata_, where_, what, &result)));
    int64_t result64 = static_cast<int64_t>(result);
    if (result != static_cast<double>(result64)) {
      return absl::InternalError(
          absl::StrCat("Error converting double attribute: ", what,
                       "with value: ", result, " to int64_t exactly."));
    }
    return result64;
  }

  absl::StatusOr<bool> get_bool(int what) const {
    int result;
    RETURN_IF_ERROR(AsStatus(GRBcbget(cbdata_, where_, what, &result)));
    bool result_bool = static_cast<bool>(result);
    if (result != static_cast<int>(result_bool)) {
      return absl::InternalError(
          absl::StrCat("Error converting int attribute: ", what,
                       "with value: ", result, " to bool exactly."));
    }
    return result_bool;
  }

  absl::StatusOr<std::string> get_string(int what) const {
    char* result;
    RETURN_IF_ERROR(AsStatus(GRBcbget(cbdata_, where_, what, &result)));
    return result;
  }

  // The output argument doubles_out will be modified, it is the callers
  // responsibility to ensure that it is large enough.
  absl::Status get_doubles(int what, absl::Span<double> doubles_out) const {
    double* const first = doubles_out.data();
    RETURN_IF_ERROR(AsStatus(GRBcbget(cbdata_, where_, what, first)));
    return absl::OkStatus();
  }

  GRBmodel* grb_model() const { return model_; }
  int where() const { return where_; }

  absl::Status AddConstraint(absl::Span<const int> vars,
                             absl::Span<const double> coefs, char sense,
                             double rhs, bool is_lazy) const {
    auto cut_fn = is_lazy ? &GRBcblazy : &GRBcbcut;
    return AsStatus((*cut_fn)(cbdata_, vars.size(), vars.begin(), coefs.begin(),
                              sense, rhs));
  }

  absl::StatusOr<double> SuggestSolution(absl::Span<const double> coefs) const {
    double obj_value;
    RETURN_IF_ERROR(
        AsStatus(GRBcbsolution(cbdata_, coefs.begin(), &obj_value)));
    return obj_value;
  }

 private:
  absl::Status AsStatus(int error_code) const {
    return GurobiStatus(model_, error_code);
  }

  GRBmodel* const model_;
  void* const cbdata_;
  const int where_;
};

// Invokes setter on a non-error value in statusor or returns the error.
//
// Requires that statusor is a StatusOr<T> and setter(T) is a function.
#define MO_SET_OR_RET(setter, statusor)                                      \
  do {                                                                       \
    auto eval_status_or = statusor;                                          \
    RETURN_IF_ERROR(eval_status_or.status()) << __FILE__ << ":" << __LINE__; \
    setter(*std::move(eval_status_or));                                      \
  } while (false)

// Sets the CallbackDataProto.runtime field using the difference between the
// current wall clock time and the start time of the solve.
absl::Status SetRuntime(const GurobiCallbackInput& callback_input,
                        CallbackDataProto& callback_data) {
  return util_time::EncodeGoogleApiProto(absl::Now() - callback_input.start,
                                         callback_data.mutable_runtime());
}

// Returns the data for the next callback. Returns nullopt if no callback is
// needed.
absl::StatusOr<absl::optional<CallbackDataProto>> CreateCallbackDataProto(
    const GurobiCallbackContext& c, const GurobiCallbackInput& callback_input,
    MessageCallbackData& message_callback_data) {
  CallbackDataProto callback_data;

  // Query information from Gurobi.
  switch (c.where()) {
    case GRB_CB_POLLING: {
      callback_data.set_event(CALLBACK_EVENT_POLLING);
      break;
    }
    case GRB_CB_PRESOLVE: {
      callback_data.set_event(CALLBACK_EVENT_PRESOLVE);
      CallbackDataProto::PresolveStats* const s =
          callback_data.mutable_presolve_stats();
      MO_SET_OR_RET(s->set_removed_variables, c.get_int(GRB_CB_PRE_COLDEL));
      MO_SET_OR_RET(s->set_removed_constraints, c.get_int(GRB_CB_PRE_ROWDEL));
      MO_SET_OR_RET(s->set_bound_changes, c.get_int(GRB_CB_PRE_BNDCHG));
      MO_SET_OR_RET(s->set_coefficient_changes, c.get_int(GRB_CB_PRE_COECHG));
      break;
    }
    case GRB_CB_SIMPLEX: {
      callback_data.set_event(CALLBACK_EVENT_SIMPLEX);
      CallbackDataProto::SimplexStats* const s =
          callback_data.mutable_simplex_stats();
      MO_SET_OR_RET(s->set_iteration_count, c.get_int64(GRB_CB_SPX_ITRCNT));
      MO_SET_OR_RET(s->set_is_pertubated, c.get_bool(GRB_CB_SPX_ISPERT));
      MO_SET_OR_RET(s->set_objective_value, c.get_double(GRB_CB_SPX_OBJVAL));
      MO_SET_OR_RET(s->set_primal_infeasibility,
                    c.get_double(GRB_CB_SPX_PRIMINF));
      MO_SET_OR_RET(s->set_dual_infeasibility,
                    c.get_double(GRB_CB_SPX_DUALINF));
      break;
    }
    case GRB_CB_BARRIER: {
      callback_data.set_event(CALLBACK_EVENT_BARRIER);
      CallbackDataProto::BarrierStats* const s =
          callback_data.mutable_barrier_stats();
      MO_SET_OR_RET(s->set_iteration_count, c.get_int(GRB_CB_BARRIER_ITRCNT));
      MO_SET_OR_RET(s->set_primal_objective,
                    c.get_double(GRB_CB_BARRIER_PRIMOBJ));
      MO_SET_OR_RET(s->set_dual_objective,
                    c.get_double(GRB_CB_BARRIER_DUALOBJ));
      MO_SET_OR_RET(s->set_primal_infeasibility,
                    c.get_double(GRB_CB_BARRIER_PRIMINF));
      MO_SET_OR_RET(s->set_dual_infeasibility,
                    c.get_double(GRB_CB_BARRIER_DUALINF));
      MO_SET_OR_RET(s->set_complementarity, c.get_double(GRB_CB_BARRIER_COMPL));
      break;
    }
    case GRB_CB_MESSAGE: {
      const absl::StatusOr<std::string> msg = c.get_string(GRB_CB_MSG_STRING);
      RETURN_IF_ERROR(msg.status())
          << "Error getting message string in callback";
      absl::optional<CallbackDataProto> message_data =
          message_callback_data.Parse(*msg);
      if (!message_data) {
        // We don't generate any callback when there is no message.
        return absl::nullopt;
      }
      callback_data = std::move(*message_data);
      break;
    }
    case GRB_CB_MIP: {
      callback_data.set_event(CALLBACK_EVENT_MIP);
      CallbackDataProto::MipStats* const s = callback_data.mutable_mip_stats();
      MO_SET_OR_RET(s->set_primal_bound, c.get_double(GRB_CB_MIP_OBJBST));
      MO_SET_OR_RET(s->set_dual_bound, c.get_double(GRB_CB_MIP_OBJBND));
      MO_SET_OR_RET(s->set_explored_nodes, c.get_int64(GRB_CB_MIP_NODCNT));
      MO_SET_OR_RET(s->set_open_nodes, c.get_int64(GRB_CB_MIP_NODLFT));
      MO_SET_OR_RET(s->set_simplex_iterations, c.get_int64(GRB_CB_MIP_ITRCNT));
      MO_SET_OR_RET(s->set_number_of_solutions_found,
                    c.get_int(GRB_CB_MIP_SOLCNT));
      MO_SET_OR_RET(s->set_cutting_planes_in_lp, c.get_int(GRB_CB_MIP_CUTCNT));

      break;
    }
    case GRB_CB_MIPSOL: {
      callback_data.set_event(CALLBACK_EVENT_MIP_SOLUTION);
      CallbackDataProto::MipStats* const s = callback_data.mutable_mip_stats();
      MO_SET_OR_RET(s->set_primal_bound, c.get_double(GRB_CB_MIPSOL_OBJBST));
      MO_SET_OR_RET(s->set_dual_bound, c.get_double(GRB_CB_MIPSOL_OBJBND));
      MO_SET_OR_RET(s->set_explored_nodes, c.get_int64(GRB_CB_MIPSOL_NODCNT));
      MO_SET_OR_RET(s->set_number_of_solutions_found,
                    c.get_int(GRB_CB_MIPSOL_SOLCNT));
      std::vector<double> var_values(callback_input.num_gurobi_vars);
      RETURN_IF_ERROR(
          c.get_doubles(GRB_CB_MIPSOL_SOL, absl::MakeSpan(var_values)))
          << "Error reading solution at event MIP_SOLUTION";
      PrimalSolutionProto* const solution =
          callback_data.mutable_primal_solution();
      *solution->mutable_variable_values() =
          ApplyFilter(var_values, callback_input.variable_ids,
                      callback_input.mip_solution_filter);
      MO_SET_OR_RET(solution->set_objective_value,
                    c.get_double(GRB_CB_MIPSOL_OBJ));
      break;
    }
    case GRB_CB_MIPNODE: {
      callback_data.set_event(CALLBACK_EVENT_MIP_NODE);
      CallbackDataProto::MipStats* const s = callback_data.mutable_mip_stats();
      MO_SET_OR_RET(s->set_primal_bound, c.get_double(GRB_CB_MIPNODE_OBJBST));
      MO_SET_OR_RET(s->set_dual_bound, c.get_double(GRB_CB_MIPNODE_OBJBND));
      MO_SET_OR_RET(s->set_explored_nodes, c.get_int64(GRB_CB_MIPNODE_NODCNT));

      MO_SET_OR_RET(s->set_number_of_solutions_found,
                    c.get_int(GRB_CB_MIPNODE_SOLCNT));
      const absl::StatusOr<int> grb_status = c.get_int(GRB_CB_MIPNODE_STATUS);
      RETURN_IF_ERROR(grb_status.status())
          << "Error reading solution status at event MIP_NODE";
      if (*grb_status == GRB_OPTIMAL) {
        std::vector<double> var_values(callback_input.num_gurobi_vars);
        RETURN_IF_ERROR(
            c.get_doubles(GRB_CB_MIPNODE_REL, absl::MakeSpan(var_values)))
            << "Error reading solution at event MIP_NODE";
        *callback_data.mutable_primal_solution()->mutable_variable_values() =
            ApplyFilter(var_values, callback_input.variable_ids,
                        callback_input.mip_node_filter);
        // Note: Gurobi does not offer an objective value for the LP relaxation.
      }
      break;
    }
    default:
      LOG(FATAL) << "Unknown gurobi callback code " << c.where();
  }

  RETURN_IF_ERROR(SetRuntime(callback_input, callback_data))
      << "Error encoding runtime at callback event: " << c.where();

  return callback_data;
}

#undef MO_SET_OR_RET

absl::Status ApplyResult(const GurobiCallbackContext& context,
                         const GurobiCallbackInput& callback_input,
                         const CallbackResultProto& result) {
  for (const CallbackResultProto::GeneratedLinearConstraint& cut :
       result.cuts()) {
    std::vector<int> gurobi_vars;
    gurobi_vars.reserve(cut.linear_expression().ids_size());
    for (const int64_t id : cut.linear_expression().ids()) {
      gurobi_vars.push_back(callback_input.variable_ids.at(id));
    }
    std::vector<std::pair<char, double>> sense_bound_pairs;
    if (cut.lower_bound() == cut.upper_bound()) {
      sense_bound_pairs.emplace_back(GRB_EQUAL, cut.upper_bound());
    } else {
      if (cut.upper_bound() < kInf) {
        sense_bound_pairs.emplace_back(GRB_LESS_EQUAL, cut.upper_bound());
      }
      if (cut.lower_bound() > -kInf) {
        sense_bound_pairs.emplace_back(GRB_GREATER_EQUAL, cut.lower_bound());
      }
    }
    for (const auto [sense, bound] : sense_bound_pairs) {
      RETURN_IF_ERROR(context.AddConstraint(gurobi_vars,
                                            cut.linear_expression().values(),
                                            sense, bound, cut.is_lazy()));
    }
  }
  for (const PrimalSolutionProto& solution : result.suggested_solution()) {
    // TODO(b/175829773): we cannot fill in auxiliary variables from range
    // constraints.
    std::vector<double> gurobi_var_values(callback_input.num_gurobi_vars,
                                          GRB_UNDEFINED);
    for (const auto [id, value] : MakeView(solution.variable_values())) {
      gurobi_var_values[callback_input.variable_ids.at(id)] = value;
    }
    RETURN_IF_ERROR(context.SuggestSolution(gurobi_var_values).status());
  }

  if (result.terminate()) {
    GRBterminate(context.grb_model());
    return absl::OkStatus();
  }
  return absl::OkStatus();
}

}  // namespace

std::vector<bool> EventToGurobiWhere(
    const absl::flat_hash_set<CallbackEventProto>& events) {
  std::vector<bool> result(kNumGurobiEvents);
  for (const auto event : events) {
    result[GurobiEvent(event)] = true;
  }
  return result;
}

absl::Status GurobiCallbackImpl(GRBmodel* grb_model, void* cbdata, int where,
                                const GurobiCallbackInput& callback_input,
                                MessageCallbackData& message_callback_data) {
  if (callback_input.user_cb == nullptr || !callback_input.events[where]) {
    return absl::OkStatus();
  }
  const GurobiCallbackContext cb_context(grb_model, cbdata, where);
  ASSIGN_OR_RETURN(const absl::optional<CallbackDataProto> callback_data,
                   CreateCallbackDataProto(cb_context, callback_input,
                                           message_callback_data));
  if (!callback_data) {
    return absl::OkStatus();
  }
  const absl::StatusOr<CallbackResultProto> result =
      callback_input.user_cb(*callback_data);
  if (!result.ok()) {
    GRBterminate(grb_model);
    return result.status();
  }
  RETURN_IF_ERROR(ApplyResult(cb_context, callback_input, *result));
  return absl::OkStatus();
}

absl::Status GurobiCallbackImplFlush(
    const GurobiCallbackInput& callback_input,
    MessageCallbackData& message_callback_data) {
  absl::optional<CallbackDataProto> callback_data =
      message_callback_data.Flush();
  if (!callback_data) {
    return absl::OkStatus();
  }

  RETURN_IF_ERROR(SetRuntime(callback_input, *callback_data))
      << "Error encoding runtime when flushing the remaining callbacks";

  // No need to terminate here, we are already done. On top of that we are after
  // the solve, so nothing in the CallbackResultProto matters.
  return callback_input.user_cb(*callback_data).status();
}

}  // namespace math_opt
}  // namespace operations_research
