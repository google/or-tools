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

#include "ortools/math_opt/solvers/xpress_solver.h"

#include <algorithm>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "absl/log/check.h"
#include "absl/memory/memory.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_join.h"
#include "absl/time/clock.h"
#include "absl/time/time.h"
#include "absl/types/span.h"
#include "ortools/base/map_util.h"
#include "ortools/base/protoutil.h"
#include "ortools/base/status_macros.h"
#include "ortools/math_opt/core/inverted_bounds.h"
#include "ortools/math_opt/core/math_opt_proto_utils.h"
#include "ortools/math_opt/core/solver_interface.h"
#include "ortools/math_opt/core/sparse_vector_view.h"
#include "ortools/math_opt/cpp/message_callback.h"
#include "ortools/math_opt/solvers/xpress/g_xpress.h"
#include "ortools/math_opt/validators/callback_validator.h"
#include "ortools/port/proto_utils.h"
#include "ortools/third_party_solvers/xpress_environment.h"
#include "ortools/util/solve_interrupter.h"

namespace operations_research {
namespace math_opt {
namespace {

class SolveContext; // forward

/** Registered callback that is auto-removed in the destructor.
 * Use Add() to add a callback to a solve context.
 * The class also provides convenience functions SetCallbackException()
 * and Interrupt() that are required in every callback implementation to
 * capture exceptions from user code and reraise them appropriately.
 */
template <typename T>
class ScopedCallback {
 private:
  ScopedCallback(ScopedCallback<T> const&) = delete;
  ScopedCallback(ScopedCallback<T>&&) = delete;
  ScopedCallback<T>& operator=(ScopedCallback<T> const&) = delete;
  ScopedCallback<T>& operator=(ScopedCallback<T>&&) = delete;
  SolveContext* context;

 public:
  typename T::DataType callbackData; /**< OR tools callback function. */
  ScopedCallback() : context(nullptr), callbackData(nullptr) {}
  // Below functions cannot be defined here since they need the definition
  // of SolveContext available.
  inline absl::Status Add(SolveContext* ctx, typename T::DataType data);
  inline void Interrupt(int reason);
  inline void SetCallbackException(std::exception_ptr ex);
  ~ScopedCallback();
};

/** Define everything required for supporting a callback of type name.
 * Use like so
 *    DEFINE_CALLBACK(CallbackName, ORToolsData, XpressReturn, (...)) {
 *        <code>
 *    }
 * where
 *    CallbackName  is the name of the callback (Message, Checktime, ...)
 *    ORToolsData   the OR tools data that is required to forward the
 *                  callback invocation from the low-level Xpress callback
 *                  to OR tools.
 *    XpressReturn  return type of the low-level Xpress callback
 *    (...)         arguments to the Xpress low-level callback.
 *    <code>        code for the low-level Xpress callback
 * The effect of the macro is a function Add##name##Callback that adds
 * the callback and returns either an error or a guard that will remove the
 * callback in its destructor. Use this like
 *   ASSIGN_OR_RETURN(auto guard, Add##name##Callback(...));
 */
#define DEFINE_CALLBACK(name, datatype, ret, args)                           \
  static ret name##Traits_LowLevelCallback args;                             \
  struct name##Traits {                                                      \
    typedef datatype DataType;                                               \
    static absl::Status Add(Xpress* xpress, void* data) {                    \
      return xpress->AddCb##name(name##Traits_LowLevelCallback, data, 0);    \
    }                                                                        \
    static void Remove(Xpress* xpress, void* data) {                         \
      CHECK_OK(xpress->RemoveCb##name(name##Traits_LowLevelCallback, data)); \
    }                                                                        \
  };                                                                         \
  static ret name##Traits_LowLevelCallback args

/** Define the message callback.
 * This forwards messages from Xpress to an ortools message callback.
 */
DEFINE_CALLBACK(Message, MessageCallback, void,
                (XPRSprob cbprob, void* cbdata, char const* msg, int len,
                 int type)) {
  ScopedCallback<MessageTraits>* cb =
      reinterpret_cast<ScopedCallback<MessageTraits>*>(cbdata);
  try {
    if (type == 1 ||  // info message
        type == 3 ||  // warning message
        type == 4)    // error message
    // message type 2 is not used by Xpress, negative values mean "flush"
    {
      if (len == 0) {
        cb->callbackData(std::vector<std::string>{{""}});
      } else {
        std::vector<std::string> lines;
        int start = 0;
        // There are a few Xpress messages that span multiple lines.
        // The MessageCallback contract says that messages must not contain
        // newlines, so we have to split on newline.
        while (start <=
               len) {  // <= rather than < to catch message ending in '\n'
          int end = start;
          while (end < len && msg[end] != '\n') ++end;
          if (start < len)
            lines.emplace_back(std::string(msg, start, end - start));
          else
            lines.push_back("");
          start = end + 1;
        }
        cb->callbackData(lines);
      }
    }
  } catch (...) {
    cb->Interrupt(XPRS_STOP_USER);
    cb->SetCallbackException(std::current_exception());
  }
}

/** Define the checktime callback.
 * This callbacks checks an interrupter for whether the solve was interrupted.
 */
DEFINE_CALLBACK(Checktime, SolveInterrupter const*, int,
                (XPRSprob cbprob, void* cbdata)) {
  ScopedCallback<ChecktimeTraits>* cb =
      reinterpret_cast<ScopedCallback<ChecktimeTraits>*>(cbdata);
  try {
    return cb->callbackData->IsInterrupted() ? 1 : 0;
  } catch (...) {
    cb->Interrupt(XPRS_STOP_USER);
    cb->SetCallbackException(std::current_exception());
    return 1;
  }
}

/** Temporary settings for a solve.
 * Instances of this class capture settings in the XPRSprob instance that are
 * made only temporarily for a solve.
 * This includes for example callbacks.
 * This is a RAII class that will undo all settings when it goes out of scope.
 */
class SolveContext {
  /** Mutex for accessing callbackException. */
  absl::Mutex mutable mutex;
  /** Capturing of exceptions in callbacks.
   * We cannot let exceptions escape from callbacks since that would just
   * unroll the stack until some function that catches the exception.
   * In particular, it would bypass any cleanup code implemented in the C code
   * of the solver. So we must capture exceptions, interrupt the solve and
   * handle the exception once the solver returned.
   */
  std::exception_ptr mutable callbackException;
  /** Installed message callback (if any). */
  ScopedCallback<MessageTraits> messageCallback;
  /** Installed interrupter (if any). */
  ScopedCallback<ChecktimeTraits> checktimeCallback;
  /** If we installed an interrupter callback then this removes it. */
  std::function<void()> removeInterrupterCallback;

 public:
  Xpress* const xpress;
  SolveContext(Xpress* xpress)
      : xpress(xpress), removeInterrupterCallback(nullptr) {}
  absl::Status AddCallbacks(MessageCallback message_callback,
                            const SolveInterrupter* interrupter) {
    if (message_callback)
      RETURN_IF_ERROR(messageCallback.Add(this, message_callback));
    if (interrupter) {
      /* To be extra safe we add two ways to interrupt Xpress:
       * 1. We register a checktime callback that polls the interrupter.
       * 2. We register a callback with the interrupter that will call
       *    XPRSinterrupt().
       * Eventually we should assess whether the first thing is a performance
       * hit and if so, remove it.
       */
      RETURN_IF_ERROR(checktimeCallback.Add(this, interrupter));
      SolveInterrupter::CallbackId const id =
          interrupter->AddInterruptionCallback(
              [=] { CHECK_OK(xpress->Interrupt(XPRS_STOP_USER)); });
      removeInterrupterCallback = [=] {
        interrupter->RemoveInterruptionCallback(id);
      };
      /** TODO: Support
       *        CallbackRegistrationProto and Callback and install the
       *        ortools callback as required.
       *        Note that this is only for Solve(), not for
       *        ComputeInfeasibleSubsystem()
       */
    }
    return absl::OkStatus();
  }
  /** TODO: Implement this.
   * absl::Status ApplyParameters(const SolveParametersProto& parameters);
   */
  /** TODO: Implement this (only for Solve(), not for
   *        ComputeInfeasibleSubsystem())
   * absl::Status ApplyParameters(const ModelSolveParametersProto&
   * model_parameters);
   */
  /** Interrupt the current solve with the given reason. */
  void Interrupt(int reason) { CHECK_OK(xpress->Interrupt(reason)); }
  /** Reraise any pending exception from a callback. */
  void ReraiseCallbackException() {
    if (callbackException) {
      std::exception_ptr old = callbackException;
      callbackException = nullptr;
      std::rethrow_exception(old);
    }
  }
  /** Set exception raised in callback.
   * Will not overwrite an existing pending exception.
   */
  void SetCallbackException(std::exception_ptr ex) {
    const absl::MutexLock lock(&mutex);
    if (!callbackException) callbackException = ex;
  }

  ~SolveContext() {
    if (removeInterrupterCallback) removeInterrupterCallback();
    // If pending callback exception was not reraised yet then do it now
    if (callbackException) std::rethrow_exception(callbackException);
  }
};

template <typename T>
absl::Status ScopedCallback<T>::Add(SolveContext* ctx,
                                    typename T::DataType data) {
  RETURN_IF_ERROR(T::Add(ctx->xpress, reinterpret_cast<void*>(this)));
  callbackData = data;
  context = ctx;
  return absl::OkStatus();
}
template <typename T>
void ScopedCallback<T>::Interrupt(int reason) {
  context->Interrupt(reason);
}
template <typename T>
void ScopedCallback<T>::SetCallbackException(std::exception_ptr ex) {
  context->SetCallbackException(ex);
}

template <typename T>
ScopedCallback<T>::~ScopedCallback() {
  if (context) T::Remove(context->xpress, reinterpret_cast<void*>(this));
}

absl::Status CheckParameters(const SolveParametersProto& parameters) {
  std::vector<std::string> warnings;
  if (parameters.has_threads() && parameters.threads() > 1) {
    warnings.push_back(absl::StrCat(
        "XpressSolver only supports parameters.threads = 1; value ",
        parameters.threads(), " is not supported"));
  }
  if (parameters.lp_algorithm() != LP_ALGORITHM_UNSPECIFIED &&
      parameters.lp_algorithm() != LP_ALGORITHM_PRIMAL_SIMPLEX &&
      parameters.lp_algorithm() != LP_ALGORITHM_DUAL_SIMPLEX &&
      parameters.lp_algorithm() != LP_ALGORITHM_BARRIER) {
    warnings.emplace_back(absl::StrCat(
        "XpressSolver does not support the 'lp_algorithm' parameter value: ",
        ProtoEnumToString(parameters.lp_algorithm())));
  }
  if (parameters.has_objective_limit()) {
    warnings.emplace_back("XpressSolver does not support objective_limit yet");
  }
  if (parameters.has_best_bound_limit()) {
    warnings.emplace_back("XpressSolver does not support best_bound_limit yet");
  }
  if (parameters.has_cutoff_limit()) {
    warnings.emplace_back("XpressSolver does not support cutoff_limit yet");
  }
  if (!warnings.empty()) {
    return absl::InvalidArgumentError(absl::StrJoin(warnings, "; "));
  }
  return absl::OkStatus();
}
}  // namespace

constexpr SupportedProblemStructures kXpressSupportedStructures = {
    .integer_variables = SupportType::kNotSupported,
    .multi_objectives = SupportType::kNotSupported,
    .quadratic_objectives = SupportType::kSupported,
    .quadratic_constraints = SupportType::kNotSupported,
    .second_order_cone_constraints = SupportType::kNotSupported,
    .sos1_constraints = SupportType::kNotSupported,
    .sos2_constraints = SupportType::kNotSupported,
    .indicator_constraints = SupportType::kNotSupported};

absl::StatusOr<std::unique_ptr<XpressSolver>> XpressSolver::New(
    const ModelProto& model, const InitArgs&) {
  if (!XpressIsCorrectlyInstalled()) {
    return absl::InvalidArgumentError("Xpress is not correctly installed.");
  }
  RETURN_IF_ERROR(
      ModelIsSupported(model, kXpressSupportedStructures, "XPRESS"));

  // We can add here extra checks that are not made in ModelIsSupported
  // (for example, if XPRESS does not support multi-objective with quad terms)

  ASSIGN_OR_RETURN(auto xpr, Xpress::New(model.name()));
  auto xpress_solver = absl::WrapUnique(new XpressSolver(std::move(xpr)));
  RETURN_IF_ERROR(xpress_solver->LoadModel(model));
  return xpress_solver;
}

absl::Status XpressSolver::LoadModel(const ModelProto& input_model) {
  CHECK(xpress_ != nullptr);
  RETURN_IF_ERROR(xpress_->SetProbName(input_model.name()));
  RETURN_IF_ERROR(AddNewVariables(input_model.variables()));
  RETURN_IF_ERROR(AddNewLinearConstraints(input_model.linear_constraints()));
  RETURN_IF_ERROR(ChangeCoefficients(input_model.linear_constraint_matrix()));
  RETURN_IF_ERROR(AddSingleObjective(input_model.objective()));
  return absl::OkStatus();
}
absl::Status XpressSolver::AddNewVariables(
    const VariablesProto& new_variables) {
  const int num_new_variables = new_variables.lower_bounds().size();
  std::vector<char> variable_type(num_new_variables);
  int n_variables = xpress_->GetNumberOfVariables();
  for (int j = 0; j < num_new_variables; ++j) {
    const VarId id = new_variables.ids(j);
    gtl::InsertOrDie(&variables_map_, id, j + n_variables);
    variable_type[j] =
        new_variables.integers(j) ? XPRS_INTEGER : XPRS_CONTINUOUS;
    if (new_variables.integers(j)) {
      is_mip_ = true;
      return absl::UnimplementedError("XpressSolver does not handle MIPs yet");
    }
  }
  RETURN_IF_ERROR(xpress_->AddVars({}, new_variables.lower_bounds(),
                                   new_variables.upper_bounds(),
                                   variable_type));

  // Not adding names for performance (have to call XPRSaddnames)
  // TODO: keep names in a cache and add them when needed

  return absl::OkStatus();
}

XpressSolver::XpressSolver(std::unique_ptr<Xpress> g_xpress)
    : xpress_(std::move(g_xpress)) {}

absl::Status XpressSolver::AddNewLinearConstraints(
    const LinearConstraintsProto& constraints) {
  // TODO: we might be able to improve performance by setting coefs also
  const int num_new_constraints = constraints.lower_bounds().size();
  std::vector<char> constraint_sense;
  constraint_sense.reserve(num_new_constraints);
  std::vector<double> constraint_rhs;
  constraint_rhs.reserve(num_new_constraints);
  std::vector<double> constraint_rng;
  constraint_rng.reserve(num_new_constraints);
  int n_constraints = xpress_->GetNumberOfConstraints();
  for (int i = 0; i < num_new_constraints; ++i) {
    const int64_t id = constraints.ids(i);
    LinearConstraintData& constraint_data =
        gtl::InsertKeyOrDie(&linear_constraints_map_, id);
    const double lb = constraints.lower_bounds(i);
    const double ub = constraints.upper_bounds(i);
    constraint_data.lower_bound = lb;
    constraint_data.upper_bound = ub;
    constraint_data.constraint_index = i + n_constraints;
    char sense = XPRS_EQUAL;
    double rhs = 0.0;
    double rng = 0.0;
    const bool lb_is_xprs_neg_inf = lb <= kMinusInf;
    const bool ub_is_xprs_pos_inf = ub >= kPlusInf;
    if (lb_is_xprs_neg_inf && !ub_is_xprs_pos_inf) {
      sense = XPRS_LESS_EQUAL;
      rhs = ub;
    } else if (!lb_is_xprs_neg_inf && ub_is_xprs_pos_inf) {
      sense = XPRS_GREATER_EQUAL;
      rhs = lb;
    } else if (lb == ub) {
      sense = XPRS_EQUAL;
      rhs = lb;
    } else {
      sense = XPRS_RANGE;
      rhs = ub;
      rng = ub - lb;
    }
    constraint_sense.emplace_back(sense);
    constraint_rhs.emplace_back(rhs);
    constraint_rng.emplace_back(rng);
  }
  // Add all constraints in one call.
  return xpress_->AddConstrs(constraint_sense, constraint_rhs, constraint_rng);
}

absl::Status XpressSolver::AddSingleObjective(const ObjectiveProto& objective) {
  // Sense
  RETURN_IF_ERROR(xpress_->SetObjectiveSense(objective.maximize()));
  is_maximize_ = objective.maximize();
  // Linear terms
  std::vector<int> index;
  index.reserve(objective.linear_coefficients().ids_size());
  for (const int64_t id : objective.linear_coefficients().ids()) {
    index.push_back(variables_map_.at(id));
  }
  RETURN_IF_ERROR(xpress_->SetLinearObjective(
      objective.offset(), index, objective.linear_coefficients().values()));
  // Quadratic terms
  const int num_terms = objective.quadratic_coefficients().row_ids().size();
  if (num_terms > 0) {
    std::vector<int> first_var_index(num_terms);
    std::vector<int> second_var_index(num_terms);
    std::vector<double> coefficients(num_terms);
    for (int k = 0; k < num_terms; ++k) {
      const int64_t row_id = objective.quadratic_coefficients().row_ids(k);
      const int64_t column_id =
          objective.quadratic_coefficients().column_ids(k);
      first_var_index[k] = variables_map_.at(row_id);
      second_var_index[k] = variables_map_.at(column_id);
      // XPRESS supposes a 1/2 implicit multiplier to quadratic terms (see doc)
      // We have to multiply it by 2 for diagonal terms
      double m = first_var_index[k] == second_var_index[k] ? 2 : 1;
      coefficients[k] = objective.quadratic_coefficients().coefficients(k) * m;
    }
    RETURN_IF_ERROR(xpress_->SetQuadraticObjective(
        first_var_index, second_var_index, coefficients));
  }
  return absl::OkStatus();
}

absl::Status XpressSolver::ChangeCoefficients(
    const SparseDoubleMatrixProto& matrix) {
  const int num_coefficients = matrix.row_ids().size();
  std::vector<int> row_index;
  row_index.reserve(num_coefficients);
  std::vector<int> col_index;
  col_index.reserve(num_coefficients);
  for (int k = 0; k < num_coefficients; ++k) {
    row_index.push_back(
        linear_constraints_map_.at(matrix.row_ids(k)).constraint_index);
    col_index.push_back(variables_map_.at(matrix.column_ids(k)));
  }
  return xpress_->ChgCoeffs(row_index, col_index, matrix.coefficients());
}

absl::StatusOr<SolveResultProto> XpressSolver::Solve(
    const SolveParametersProto& parameters,
    const ModelSolveParametersProto& model_parameters,
    MessageCallback message_callback,
    const CallbackRegistrationProto& callback_registration, Callback,
    const SolveInterrupter* interrupter) {
  RETURN_IF_ERROR(ModelSolveParametersAreSupported(
      model_parameters, kXpressSupportedStructures, "XPRESS"));
  const absl::Time start = absl::Now();

  RETURN_IF_ERROR(CheckRegisteredCallbackEvents(callback_registration,
                                                /*supported_events=*/{}));

  RETURN_IF_ERROR(CheckParameters(parameters));

  SolveContext solveContext(xpress_.get());
  RETURN_IF_ERROR(solveContext.AddCallbacks(message_callback, interrupter));

  // Check that bounds are not inverted just before solve
  // XPRESS returns "infeasible" when bounds are inverted
  {
    ASSIGN_OR_RETURN(const InvertedBounds inverted_bounds,
                     ListInvertedBounds());
    RETURN_IF_ERROR(inverted_bounds.ToStatus());
  }

  // Set initial basis
  if (model_parameters.has_initial_basis()) {
    RETURN_IF_ERROR(SetXpressStartingBasis(model_parameters.initial_basis()));
  }

  RETURN_IF_ERROR(CallXpressSolve(parameters)) << "Error during XPRESS solve";
  solveContext.ReraiseCallbackException();

  ASSIGN_OR_RETURN(
      SolveResultProto solve_result,
      ExtractSolveResultProto(start, model_parameters, parameters));

  return solve_result;
}

std::string XpressSolver::GetLpOptimizationFlags(
    const SolveParametersProto& parameters) {
  switch (parameters.lp_algorithm()) {
    case LP_ALGORITHM_PRIMAL_SIMPLEX:
      lp_algorithm_ = LP_ALGORITHM_PRIMAL_SIMPLEX;
      return "p";
    case LP_ALGORITHM_DUAL_SIMPLEX:
      lp_algorithm_ = LP_ALGORITHM_DUAL_SIMPLEX;
      return "d";
    case LP_ALGORITHM_BARRIER:
      lp_algorithm_ = LP_ALGORITHM_BARRIER;
      return "b";
    default:
      // this makes XPRESS use default algorithm (XPRS_DEFAULTALG)
      // but we have to figure out what it is for solution processing
      auto default_alg = xpress_->GetIntControl(XPRS_DEFAULTALG);
      switch (default_alg.value_or(-1)) {
        case XPRS_ALG_PRIMAL:
          lp_algorithm_ = LP_ALGORITHM_PRIMAL_SIMPLEX;
          break;
        case XPRS_ALG_DUAL:
          lp_algorithm_ = LP_ALGORITHM_DUAL_SIMPLEX;
          break;
        case XPRS_ALG_BARRIER:
          lp_algorithm_ = LP_ALGORITHM_BARRIER;
          break;
        default:
          lp_algorithm_ = LP_ALGORITHM_UNSPECIFIED;
      }
      return "";
  }
}
absl::Status XpressSolver::CallXpressSolve(
    const SolveParametersProto& parameters) {
  // Enable screen output right before solve
  if (parameters.enable_output()) {
    RETURN_IF_ERROR(xpress_->SetIntControl(XPRS_OUTPUTLOG, 1))
        << "Unable to enable XPRESS logs";
  }
  // Solve
  if (is_mip_) {
    RETURN_IF_ERROR(xpress_->MipOptimize());
    ASSIGN_OR_RETURN(xpress_mip_status_, xpress_->GetIntAttr(XPRS_MIPSTATUS));
  } else {
    RETURN_IF_ERROR(SetLpIterLimits(parameters))
        << "Could not set iteration limits.";
    RETURN_IF_ERROR(xpress_->LpOptimize(GetLpOptimizationFlags(parameters)));
    ASSIGN_OR_RETURN(int primal_status, xpress_->GetIntAttr(XPRS_LPSTATUS));
    ASSIGN_OR_RETURN(int dual_status, xpress_->GetDualStatus());
    xpress_lp_status_ = {primal_status, dual_status};
  }
  // Post-solve
  if (!(is_mip_ ? (xpress_mip_status_ == XPRS_MIP_OPTIMAL)
                : (xpress_lp_status_.primal_status == XPRS_LP_OPTIMAL))) {
    RETURN_IF_ERROR(xpress_->PostSolve()) << "Post-solve failed in XPRESS";
  }
  // Disable screen output right after solve
  if (parameters.enable_output()) {
    RETURN_IF_ERROR(xpress_->SetIntControl(XPRS_OUTPUTLOG, 0))
        << "Unable to disable XPRESS logs";
  }
  return absl::OkStatus();
}

absl::Status XpressSolver::SetLpIterLimits(
    const SolveParametersProto& parameters) {
  // If the user has set no limits, we still have to reset the limits
  // explicitly to their default values, else the parameters could be kept
  // between solves.
  if (parameters.has_iteration_limit()) {
    RETURN_IF_ERROR(xpress_->SetIntControl(
        XPRS_LPITERLIMIT, static_cast<int>(parameters.iteration_limit())))
        << "Could not set XPRS_LPITERLIMIT";
    RETURN_IF_ERROR(xpress_->SetIntControl(
        XPRS_BARITERLIMIT, static_cast<int>(parameters.iteration_limit())))
        << "Could not set XPRS_BARITERLIMIT";
  } else {
    RETURN_IF_ERROR(xpress_->ResetIntControl(XPRS_LPITERLIMIT))
        << "Could not reset XPRS_LPITERLIMIT to its default value";
    RETURN_IF_ERROR(xpress_->ResetIntControl(XPRS_BARITERLIMIT))
        << "Could not reset XPRS_BARITERLIMIT to its default value";
  }
  return absl::OkStatus();
}

absl::StatusOr<SolveResultProto> XpressSolver::ExtractSolveResultProto(
    absl::Time start, const ModelSolveParametersProto& model_parameters,
    const SolveParametersProto& solve_parameters) {
  SolveResultProto result;
  ASSIGN_OR_RETURN(SolutionProto solution,
                   GetSolution(model_parameters, solve_parameters));
  *result.add_solutions() = std::move(solution);
  ASSIGN_OR_RETURN(*result.mutable_solve_stats(), GetSolveStats(start));
  ASSIGN_OR_RETURN(const double best_primal_bound, GetBestPrimalBound());
  ASSIGN_OR_RETURN(const double best_dual_bound, GetBestDualBound());
  ASSIGN_OR_RETURN(
      *result.mutable_termination(),
      ConvertTerminationReason(best_primal_bound, best_dual_bound));
  return result;
}

absl::StatusOr<double> XpressSolver::GetBestPrimalBound() const {
  if ((lp_algorithm_ == LP_ALGORITHM_PRIMAL_SIMPLEX) &&
      (isPrimalFeasible() ||
       xpress_lp_status_.primal_status == XPRS_LP_OPTIMAL)) {
    // When primal simplex algorithm is used, XPRESS uses LPOBJVAL to store the
    // primal problem's objective value
    return xpress_->GetDoubleAttr(XPRS_LPOBJVAL);
  }
  return is_maximize_ ? kMinusInf : kPlusInf;
}

absl::StatusOr<double> XpressSolver::GetBestDualBound() const {
  if ((lp_algorithm_ == LP_ALGORITHM_DUAL_SIMPLEX) &&
      (isPrimalFeasible() ||
       xpress_lp_status_.primal_status == XPRS_LP_OPTIMAL)) {
    // When dual simplex algorithm is used, XPRESS uses LPOBJVAL to store the
    // dual problem's objective value
    return xpress_->GetDoubleAttr(XPRS_LPOBJVAL);
  }
  return is_maximize_ ? kPlusInf : kMinusInf;
}

absl::StatusOr<SolutionProto> XpressSolver::GetSolution(
    const ModelSolveParametersProto& model_parameters,
    const SolveParametersProto& solve_parameters) {
  if (is_mip_) {
    return absl::UnimplementedError("XpressSolver does not handle MIPs yet");
  } else {
    return GetLpSolution(model_parameters, solve_parameters);
  }
}

absl::StatusOr<SolutionProto> XpressSolver::GetLpSolution(
    const ModelSolveParametersProto& model_parameters,
    const SolveParametersProto& solve_parameters) {
  // Fetch all results from XPRESS
  int nVars = xpress_->GetNumberOfVariables();
  int nCons = xpress_->GetNumberOfConstraints();
  std::vector<double> primals(nVars);
  std::vector<double> duals(nCons);
  std::vector<double> reducedCosts(nVars);

  auto hasSolution =
      xpress_
          ->GetLpSol(absl::MakeSpan(primals), absl::MakeSpan(duals),
                     absl::MakeSpan(reducedCosts))
          .ok();

  SolutionProto solution{};

  if (isPrimalFeasible()) {
    // Handle primal solution
    solution.mutable_primal_solution()->set_feasibility_status(
        getLpSolutionStatus());
    ASSIGN_OR_RETURN(const double primalBound, GetBestPrimalBound());
    solution.mutable_primal_solution()->set_objective_value(primalBound);
    XpressVectorToSparseDoubleVector(
        primals, variables_map_,
        *solution.mutable_primal_solution()->mutable_variable_values(),
        model_parameters.variable_values_filter());
  }

  if (hasSolution) {
    // Add dual solution even if not feasible
    solution.mutable_dual_solution()->set_feasibility_status(
        getDualSolutionStatus());
    ASSIGN_OR_RETURN(const double dualBound, GetBestDualBound());
    solution.mutable_dual_solution()->set_objective_value(dualBound);
    XpressVectorToSparseDoubleVector(
        duals, linear_constraints_map_,
        *solution.mutable_dual_solution()->mutable_dual_values(),
        model_parameters.dual_values_filter());
    XpressVectorToSparseDoubleVector(
        reducedCosts, variables_map_,
        *solution.mutable_dual_solution()->mutable_reduced_costs(),
        model_parameters.reduced_costs_filter());
  }

  // Get basis
  ASSIGN_OR_RETURN(auto basis, GetBasisIfAvailable(solve_parameters));
  if (basis.has_value()) {
    *solution.mutable_basis() = std::move(*basis);
  }
  return solution;
}

bool XpressSolver::isPrimalFeasible() const {
  if (is_mip_) {
    return xpress_mip_status_ == XPRS_MIP_OPTIMAL ||
           xpress_mip_status_ == XPRS_MIP_SOLUTION;
  } else {
    return xpress_lp_status_.primal_status == XPRS_LP_OPTIMAL ||
           xpress_lp_status_.primal_status == XPRS_LP_UNFINISHED;
  }
}

bool XpressSolver::isDualFeasible() const {
  if (is_mip_) {
    return isPrimalFeasible();
  }
  return xpress_lp_status_.dual_status == XPRS_SOLSTATUS_OPTIMAL ||
         xpress_lp_status_.dual_status == XPRS_SOLSTATUS_FEASIBLE ||
         // When using dual simplex algorithm, if we interrupt it, dual_status
         // is "not found" even if there is a solution. Using the following
         // as a workaround for now
         (lp_algorithm_ == LP_ALGORITHM_DUAL_SIMPLEX && isPrimalFeasible());
}

SolutionStatusProto XpressSolver::getLpSolutionStatus() const {
  switch (xpress_lp_status_.primal_status) {
    case XPRS_LP_OPTIMAL:
    case XPRS_LP_UNFINISHED:
      return SOLUTION_STATUS_FEASIBLE;
    case XPRS_LP_INFEAS:
    case XPRS_LP_CUTOFF:
    case XPRS_LP_CUTOFF_IN_DUAL:
    case XPRS_LP_NONCONVEX:
      return SOLUTION_STATUS_INFEASIBLE;
    case XPRS_LP_UNSTARTED:
    case XPRS_LP_UNBOUNDED:
    case XPRS_LP_UNSOLVED:
      return SOLUTION_STATUS_UNDETERMINED;
    default:
      return SOLUTION_STATUS_UNSPECIFIED;
  }
}

SolutionStatusProto XpressSolver::getDualSolutionStatus() const {
  // When using dual simplex algorithm, if we interrupt it, dual_status
  // is "not found" even if there is a solution. Using the following
  // as a workaround for now
  if (isDualFeasible()) {
    return SOLUTION_STATUS_FEASIBLE;
  }
  switch (xpress_lp_status_.dual_status) {
    case XPRS_SOLSTATUS_OPTIMAL:
    case XPRS_SOLSTATUS_FEASIBLE:
      return SOLUTION_STATUS_FEASIBLE;
    case XPRS_SOLSTATUS_INFEASIBLE:
      return SOLUTION_STATUS_INFEASIBLE;
    case XPRS_SOLSTATUS_UNBOUNDED:
      // when primal is unbounded, XPRESS returns unbounded for dual also (known
      // issue). this is a temporary workaround
      return (xpress_lp_status_.primal_status == XPRS_LP_UNBOUNDED)
                 ? SOLUTION_STATUS_INFEASIBLE
                 : SOLUTION_STATUS_UNDETERMINED;
    case XPRS_SOLSTATUS_NOTFOUND:
      return SOLUTION_STATUS_UNDETERMINED;
    default:
      return SOLUTION_STATUS_UNSPECIFIED;
  }
}

inline BasisStatusProto XpressToMathOptBasisStatus(const int status,
                                                   bool isConstraint) {
  // XPRESS row basis status is that of the slack variable
  // For example, if the slack variable is at LB, the constraint is at UB
  switch (status) {
    case XPRS_BASIC:
      return BASIS_STATUS_BASIC;
    case XPRS_AT_LOWER:
      return isConstraint ? BASIS_STATUS_AT_UPPER_BOUND
                          : BASIS_STATUS_AT_LOWER_BOUND;
    case XPRS_AT_UPPER:
      return isConstraint ? BASIS_STATUS_AT_LOWER_BOUND
                          : BASIS_STATUS_AT_UPPER_BOUND;
    case XPRS_FREE_SUPER:
      return BASIS_STATUS_FREE;
    default:
      return BASIS_STATUS_UNSPECIFIED;
  }
}

inline int MathOptToXpressBasisStatus(const BasisStatusProto status,
                                      bool isConstraint) {
  // XPRESS row basis status is that of the slack variable
  // For example, if the slack variable is at LB, the constraint is at UB
  switch (status) {
    case BASIS_STATUS_BASIC:
      return XPRS_BASIC;
    case BASIS_STATUS_AT_LOWER_BOUND:
      return isConstraint ? XPRS_AT_UPPER : XPRS_AT_LOWER;
    case BASIS_STATUS_AT_UPPER_BOUND:
      return isConstraint ? XPRS_AT_LOWER : XPRS_AT_UPPER;
    case BASIS_STATUS_FREE:
      return XPRS_FREE_SUPER;
    default:
      return XPRS_FREE_SUPER;
  }
}

absl::Status XpressSolver::SetXpressStartingBasis(const BasisProto& basis) {
  std::vector<int> xpress_var_basis_status(xpress_->GetNumberOfVariables());
  for (const auto [id, value] : MakeView(basis.variable_status())) {
    xpress_var_basis_status[variables_map_.at(id)] =
        MathOptToXpressBasisStatus(static_cast<BasisStatusProto>(value), false);
  }
  std::vector<int> xpress_constr_basis_status(
      xpress_->GetNumberOfConstraints());
  for (const auto [id, value] : MakeView(basis.constraint_status())) {
    xpress_constr_basis_status[linear_constraints_map_.at(id)
                                   .constraint_index] =
        MathOptToXpressBasisStatus(static_cast<BasisStatusProto>(value), true);
  }
  return xpress_->SetStartingBasis(xpress_constr_basis_status,
                                   xpress_var_basis_status);
}

absl::StatusOr<std::optional<BasisProto>> XpressSolver::GetBasisIfAvailable(
    const SolveParametersProto&) {
  std::vector<int> xprs_variable_basis_status;
  std::vector<int> xprs_constraint_basis_status;
  if (!xpress_
           ->GetBasis(xprs_constraint_basis_status, xprs_variable_basis_status)
           .ok()) {
    return std::nullopt;
  }

  BasisProto basis;
  // Variable basis
  for (auto [variable_id, xprs_variable_index] : variables_map_) {
    basis.mutable_variable_status()->add_ids(variable_id);
    const BasisStatusProto variable_status = XpressToMathOptBasisStatus(
        xprs_variable_basis_status[xprs_variable_index], false);
    if (variable_status == BASIS_STATUS_UNSPECIFIED) {
      return absl::InternalError(
          absl::StrCat("Invalid Xpress variable basis status: ",
                       xprs_variable_basis_status[xprs_variable_index]));
    }
    basis.mutable_variable_status()->add_values(variable_status);
  }

  // Constraint basis
  for (auto [constraint_id, xprs_ct_index] : linear_constraints_map_) {
    basis.mutable_constraint_status()->add_ids(constraint_id);
    const BasisStatusProto status = XpressToMathOptBasisStatus(
        xprs_constraint_basis_status[xprs_ct_index.constraint_index], true);
    if (status == BASIS_STATUS_UNSPECIFIED) {
      return absl::InternalError(absl::StrCat(
          "Invalid Xpress constraint basis status: ",
          xprs_constraint_basis_status[xprs_ct_index.constraint_index]));
    }
    basis.mutable_constraint_status()->add_values(status);
  }

  // Dual basis
  basis.set_basic_dual_feasibility(
      isDualFeasible() ? SOLUTION_STATUS_FEASIBLE : SOLUTION_STATUS_INFEASIBLE);
  return basis;
}

absl::StatusOr<SolveStatsProto> XpressSolver::GetSolveStats(
    absl::Time start) const {
  SolveStatsProto solve_stats;
  CHECK_OK(util_time::EncodeGoogleApiProto(absl::Now() - start,
                                           solve_stats.mutable_solve_time()));

  // LP simplex iterations
  {
    ASSIGN_OR_RETURN(const int iters, xpress_->GetIntAttr(XPRS_SIMPLEXITER));
    solve_stats.set_simplex_iterations(iters);
  }
  // LP barrier iterations
  {
    ASSIGN_OR_RETURN(const int iters, xpress_->GetIntAttr(XPRS_BARITER));
    solve_stats.set_barrier_iterations(iters);
  }

  // TODO: complete these stats
  return solve_stats;
}

template <typename T>
void XpressSolver::XpressVectorToSparseDoubleVector(
    absl::Span<const double> xpress_values, const T& map,
    SparseDoubleVectorProto& result,
    const SparseVectorFilterProto& filter) const {
  SparseVectorFilterPredicate predicate(filter);
  for (auto [id, xpress_data] : map) {
    const double value = xpress_values[get_model_index(xpress_data)];
    if (predicate.AcceptsAndUpdate(id, value)) {
      result.add_ids(id);
      result.add_values(value);
    }
  }
}

absl::StatusOr<TerminationProto> XpressSolver::ConvertTerminationReason(
    double best_primal_bound, double best_dual_bound) const {
  if (!is_mip_) {
    switch (xpress_lp_status_.primal_status) {
      case XPRS_LP_UNSTARTED:
        return TerminateForReason(
            is_maximize_, TERMINATION_REASON_OTHER_ERROR,
            "Problem solve has not started (XPRS_LP_UNSTARTED)");
      case XPRS_LP_OPTIMAL:
        return OptimalTerminationProto(best_primal_bound, best_dual_bound);
      case XPRS_LP_INFEAS:
        return InfeasibleTerminationProto(
            is_maximize_, isDualFeasible() ? FEASIBILITY_STATUS_FEASIBLE
                                           : FEASIBILITY_STATUS_UNDETERMINED);
      case XPRS_LP_CUTOFF:
        return CutoffTerminationProto(
            is_maximize_, "Objective worse than cutoff (XPRS_LP_CUTOFF)");
      case XPRS_LP_UNFINISHED:
        // TODO: add support for more limit types here (this only works for LP
        // iterations limit for now)
        return FeasibleTerminationProto(
            is_maximize_, LIMIT_ITERATION, best_primal_bound, best_dual_bound,
            "Solve did not finish (XPRS_LP_UNFINISHED)");
      case XPRS_LP_UNBOUNDED:
        return UnboundedTerminationProto(is_maximize_,
                                         "Xpress status XPRS_LP_UNBOUNDED");
      case XPRS_LP_CUTOFF_IN_DUAL:
        return CutoffTerminationProto(
            is_maximize_, "Cutoff in dual (XPRS_LP_CUTOFF_IN_DUAL)");
      case XPRS_LP_UNSOLVED:
        return TerminateForReason(
            is_maximize_, TERMINATION_REASON_NUMERICAL_ERROR,
            "Problem could not be solved due to numerical issues "
            "(XPRS_LP_UNSOLVED)");
      case XPRS_LP_NONCONVEX:
        return TerminateForReason(is_maximize_, TERMINATION_REASON_OTHER_ERROR,
                                  "Problem contains quadratic data, which is "
                                  "not convex (XPRS_LP_NONCONVEX)");
      default:
        return absl::InternalError(
            absl::StrCat("Missing Xpress LP status code case: ",
                         xpress_lp_status_.primal_status));
    }
  } else {
    return absl::UnimplementedError("XpressSolver does not handle MIPs yet");
  }
}

absl::StatusOr<bool> XpressSolver::Update(const ModelUpdateProto&) {
  // Not implemented yet
  return false;
}

absl::StatusOr<ComputeInfeasibleSubsystemResultProto>
XpressSolver::ComputeInfeasibleSubsystem(const SolveParametersProto&,
                                         MessageCallback message_callback,
                                         const SolveInterrupter* interrupter) {
  SolveContext solveContext(xpress_.get());
  RETURN_IF_ERROR(solveContext.AddCallbacks(message_callback, interrupter));

  return absl::UnimplementedError(
      "XPRESS does not provide a method to compute an infeasible subsystem");
  /** TODO:
      solveContext.ReraiseCallbackException();
  */
}

absl::StatusOr<InvertedBounds> XpressSolver::ListInvertedBounds() const {
  InvertedBounds inverted_bounds;
  {
    ASSIGN_OR_RETURN(const std::vector<double> var_lbs, xpress_->GetVarLb());
    ASSIGN_OR_RETURN(const std::vector<double> var_ubs, xpress_->GetVarUb());
    for (const auto& [id, index] : variables_map_) {
      if (var_lbs[index] > var_ubs[index]) {
        inverted_bounds.variables.push_back(id);
      }
    }
  }
  // We could have used XPRSgetrhsrange to check if one is negative. However,
  // XPRSaddrows ignores the sign of the RHS range and takes the absolute value
  // in all cases. So we need to do the following checks on the internal model.
  for (const auto& [id, cstr_data] : linear_constraints_map_) {
    if (cstr_data.lower_bound > cstr_data.upper_bound) {
      inverted_bounds.linear_constraints.push_back(id);
    }
  }
  // Above code have inserted ids in non-stable order.
  std::sort(inverted_bounds.variables.begin(), inverted_bounds.variables.end());
  std::sort(inverted_bounds.linear_constraints.begin(),
            inverted_bounds.linear_constraints.end());
  return inverted_bounds;
}

MATH_OPT_REGISTER_SOLVER(SOLVER_TYPE_XPRESS, XpressSolver::New)
}  // namespace math_opt
}  // namespace operations_research
