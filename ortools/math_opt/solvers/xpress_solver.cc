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
#include <type_traits>
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

struct SharedSolveContext {
  Xpress* xpress;

  /** Mutex for accessing callbackException. */
  absl::Mutex mutex;

  /** Capturing of exceptions in callbacks.
   * We cannot let exceptions escape from callbacks since that would just
   * unroll the stack until some function that catches the exception.
   * In particular, it would bypass any cleanup code implemented in the C code
   * of the solver. So we must capture exceptions, interrupt the solve and
   * handle the exception once the solver returned.
   */
  std::exception_ptr callbackException;
};

/** Registered callback that is auto-removed in the destructor.
 * Use Add() to add a callback to a solve context.
 * The class also provides convenience functions SetCallbackException()
 * and Interrupt() that are required in every callback implementation to
 * capture exceptions from user code and reraise them appropriately.
 */
template <typename ProtoT, typename CbT>
class ScopedCallback {
  using proto_type = typename ProtoT::proto_type;
  SharedSolveContext* ctx;

  ScopedCallback(ScopedCallback const&) = delete;
  ScopedCallback(ScopedCallback&&) = delete;
  ScopedCallback& operator=(ScopedCallback const&) = delete;
  ScopedCallback& operator=(ScopedCallback&&) = delete;

  // We intercept and store any exception throw by a callback defining a static
  // wrapper function that invokes the callback within a try/carch block. For
  // this to work, we need to deduce the callback return type and arguments.
  template <typename FuncPtr>
  struct ExWrapper;

  // Specialization to deduce the callback return and arguments types
  template <typename R, typename... Args>
  struct ExWrapper<R (*)(XPRSprob, void*, Args...)> {
    // The static function that will be directly invoked by Xpress
    static auto low_level_cb(XPRSprob prob, void* cbdata, Args... args) try {
      return ProtoT::glueFn(prob, cbdata, args...);
    } catch (...) {
      // Catch any exception and terminate Xpress gracefully
      ScopedCallback* cb = reinterpret_cast<ScopedCallback*>(cbdata);
      cb->Interrupt(XPRS_STOP_USER);
      cb->SetCallbackException(std::current_exception());
      if constexpr (std::is_convertible_v<R, int>) return static_cast<int>(1);
    }
  };
  static constexpr proto_type low_level_cb =
      ExWrapper<proto_type>::low_level_cb;

 public:
  CbT or_tools_cb;

  ScopedCallback() : ctx(nullptr) {}

  inline absl::Status Add(SharedSolveContext* context, CbT cb) {
    ctx = context;
    RETURN_IF_ERROR(
        ProtoT::Add(ctx->xpress, low_level_cb, reinterpret_cast<void*>(this)));
    return absl::OkStatus();
  }

  inline void Interrupt(int reason) {
    CHECK_OK(ctx->xpress->Interrupt(reason));
  }

  inline void SetCallbackException(std::exception_ptr ex) {
    const absl::MutexLock lock(&ctx->mutex);
    if (!ctx->callbackException) ctx->callbackException = ex;
  }

  ~ScopedCallback() {
    if (ctx)
      ProtoT::Remove(ctx->xpress, low_level_cb, reinterpret_cast<void*>(this));
  }
};

/** Define everything required for supporting a callback of type name.
 * Use like so
 *    DEFINE_SCOPED_CB(CB_NAME, ORTOOLS_CB, CB_RET_TYPE, (...ARGS)) {
 *        <code>
 *    }
 * where
 *    CB_NAME      is the name of the callback (Message, Checktime, ...)
 *    ORTOOLS_CB   the Or-Tools callbacks (function object) that get provided
 *                 to the low-level static callback as user data, and then
 *                 invoked.
 *    CB_RET_TYPE  return type of the low-level Xpress callback.
 *    (...ARGS)    arguments to the Xpress low-level callback.
 *    <code>       code for the low-level Xpress callback
 * The effect of the macro is an alias CB_NAME####ScopedCb =
 * ScopedCallback<...>.
 */
#define DEFINE_SCOPED_CB(CB_NAME, ORTOOLS_CB, CB_RET_TYPE, ARGS)         \
  CB_RET_TYPE CB_NAME##GlueFn ARGS;                                      \
  struct CB_NAME##Traits {                                               \
    using proto_type = CB_RET_TYPE(XPRS_CC*) ARGS;                       \
    static constexpr proto_type glueFn = CB_NAME##GlueFn;                \
    static absl::Status Add(Xpress* xpress, proto_type fn, void* data) { \
      return xpress->AddCb##CB_NAME(fn, data, 0);                        \
    }                                                                    \
    static void Remove(Xpress* xpress, proto_type fn, void* data) {      \
      CHECK_OK(xpress->RemoveCb##CB_NAME(fn, data));                     \
    }                                                                    \
  };                                                                     \
  using CB_NAME##ScopedCb = ScopedCallback<CB_NAME##Traits, ORTOOLS_CB>; \
  CB_RET_TYPE CB_NAME##GlueFn ARGS

/** Define the message callback.
 * This forwards messages from Xpress to an ortools message callback.
 */
DEFINE_SCOPED_CB(Message, MessageCallback, void,
                 (XPRSprob prob, void* cbdata, char const* msg, int len,
                  int type)) {
  auto cb = reinterpret_cast<MessageScopedCb*>(cbdata);

  if (type != 1 &&  // info message
      type != 3 &&  // warning message
      type != 4) {  // error message
    // message type 2 is not used by Xpress, negative values mean "flush"
    return;
  }

  if (len == 0) {
    cb->or_tools_cb(std::vector<std::string>{""});
    return;
  }

  std::vector<std::string> lines;
  int start = 0;
  // There are a few Xpress messages that span multiple lines.
  // The MessageCallback contract says that messages must not contain
  // newlines, so we have to split on newline.
  while (start <= len) {  // <= rather than < to catch message ending in '\n'
    int end = start;
    while (end < len && msg[end] != '\n') {
      ++end;
    }
    if (start < len) {
      lines.emplace_back(msg, start, end - start);
    } else {
      lines.push_back("");
    }
    start = end + 1;
  }
  cb->or_tools_cb(lines);
}

/** Define the checktime callback.
 * This callbacks checks an interrupter for whether the solve was interrupted.
 */
DEFINE_SCOPED_CB(Checktime, SolveInterrupter const*, int,
                 (XPRSprob prob, void* cbdata)) {
  auto cb = reinterpret_cast<ChecktimeScopedCb*>(cbdata);
  // Note: we do NOT return non-zero from the callback if the solve was
  //       interrupted. Returning non-zero from the callback is interpreted
  //       as hitting a time limit and we would therefore not map correctly
  //       the resulting stop status to or tools' termination status.
  if (cb->or_tools_cb->IsInterrupted()) {
    cb->Interrupt(XPRS_STOP_USER);
  }
  return 0;
}

/** An ortools message callback that prints everything to stdout. */
static void stdoutMessageCallback(std::vector<std::string> const& lines) {
  for (auto& l : lines) std::cout << l << std::endl;
}

/** Temporary settings for a solve.
 * Instances of this class capture settings in the XPRSprob instance that are
 * made only temporarily for a solve.
 * This includes for example callbacks.
 * This is a RAII class that will undo all settings when it goes out of scope.
 */
class ScopedSolverContext {
  /** Solver context data shared by callbacks */
  SharedSolveContext shared_ctx;
  /** Installed message callback (if any). */
  MessageScopedCb messageCallback;
  /** Installed interrupter (if any). */
  ChecktimeScopedCb checktimeCallback;
  /** If we installed an interrupter callback then this removes it. */
  std::function<void()> removeInterrupterCallback;
  /** A single control that must be reset in the destructor. */
  struct OneControl {
    enum { INT_CONTROL, DBL_CONTROL, STR_CONTROL } type;
    int id;
    int64_t l;
    double d;
    std::string s;
  };
  /** Controls to be reset in the destructor. */
  std::vector<OneControl> modifiedControls;

 public:
  ScopedSolverContext(Xpress* xpress) : removeInterrupterCallback(nullptr) {
    shared_ctx.xpress = xpress;
  }
  absl::Status AddCallbacks(MessageCallback message_callback,
                            const SolveInterrupter* interrupter) {
    if (message_callback)
      RETURN_IF_ERROR(messageCallback.Add(&shared_ctx, message_callback));
    if (interrupter) {
      /* To be extra safe we add two ways to interrupt Xpress:
       * 1. We register a checktime callback that polls the interrupter.
       * 2. We register a callback with the interrupter that will call
       *    XPRSinterrupt().
       * Eventually we should assess whether the first thing is a performance
       * hit and if so, remove it.
       */
      RETURN_IF_ERROR(checktimeCallback.Add(&shared_ctx, interrupter));
      SolveInterrupter::CallbackId const id =
          interrupter->AddInterruptionCallback(
              [=] { CHECK_OK(shared_ctx.xpress->Interrupt(XPRS_STOP_USER)); });
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
  /** Setup model specific parameters. */
  absl::Status ApplyParameters(const SolveParametersProto& parameters,
                               MessageCallback message_callback) {
    std::vector<std::string> warnings;
    ASSIGN_OR_RETURN(bool const isMIP, xpress->IsMIP());
    if (parameters.enable_output()) {
      // This is considered only if no message callback is set.
      if (!message_callback) {
        RETURN_IF_ERROR(messageCallback.Add(this, stdoutMessageCallback));
      }
    }
    absl::Duration time_limit = absl::InfiniteDuration();
    if (parameters.has_time_limit()) {
      ASSIGN_OR_RETURN(
          time_limit, util_time::DecodeGoogleApiProto(parameters.time_limit()));
    }
    if (time_limit < absl::InfiniteDuration()) {
      RETURN_IF_ERROR(Set(XPRS_TIMELIMIT, absl::ToDoubleSeconds(time_limit)));
    }
    if (parameters.has_iteration_limit()) {
      if (parameters.lp_algorithm() == LP_ALGORITHM_FIRST_ORDER) {
        // Iteration limit for PDHG is BARHGMAXRESTARTS
        RETURN_IF_ERROR(
            Set(XPRS_BARHGMAXRESTARTS, parameters.iteration_limit()));
      } else {
        RETURN_IF_ERROR(Set(XPRS_LPITERLIMIT, parameters.iteration_limit()));
        RETURN_IF_ERROR(Set(XPRS_BARITERLIMIT, parameters.iteration_limit()));
      }
    }
    if (parameters.has_node_limit()) {
      RETURN_IF_ERROR(Set(XPRS_MAXNODE, parameters.node_limit()));
    }
    if (parameters.has_cutoff_limit()) {
      RETURN_IF_ERROR(Set(XPRS_MIPABSCUTOFF, parameters.cutoff_limit()));
    }
    if (parameters.has_objective_limit()) {
      // In Xpress you can apply MIPABSCUTOFF also to LPs.
      // However, or tools applies both cutoff_limit and objective_limit
      // to LPs and distinguishes the two, i.e., expect different return
      // values depending on what is set. Since we cannot easily make this
      // distinction, we do not support objective_limit. Users should just
      // use cutoff_limit with LPs as well.
      warnings.emplace_back("XpressSolver does not support objective_limit");
    }
    if (parameters.has_best_bound_limit()) {
      warnings.emplace_back("XpressSolver does not support best_bound_limit");
    }
    if (parameters.has_solution_limit()) {
      RETURN_IF_ERROR(Set(XPRS_MAXMIPSOL, parameters.solution_limit()));
    }
    if (parameters.has_threads() && parameters.threads() > 1)
      RETURN_IF_ERROR(Set(XPRS_THREADS, parameters.threads()));
    if (parameters.has_random_seed()) {
      RETURN_IF_ERROR(Set(XPRS_RANDOMSEED, parameters.random_seed()));
    }
    if (parameters.has_absolute_gap_tolerance())
      RETURN_IF_ERROR(
          Set(XPRS_MIPABSSTOP, parameters.absolute_gap_tolerance()));
    if (parameters.has_relative_gap_tolerance())
      RETURN_IF_ERROR(
          Set(XPRS_MIPRELSTOP, parameters.relative_gap_tolerance()));
    if (parameters.has_solution_pool_size()) {
      warnings.emplace_back("XpressSolver does not support solution_pool_size");
    }
    // According to the documentation, LP algorithm is only for LPs
    if (!isMIP && parameters.lp_algorithm() != LP_ALGORITHM_UNSPECIFIED) {
      switch (parameters.lp_algorithm()) {
        case LP_ALGORITHM_PRIMAL_SIMPLEX:
          RETURN_IF_ERROR(Set(XPRS_LPFLAGS, 1 << 1));
          break;
        case LP_ALGORITHM_DUAL_SIMPLEX:
          RETURN_IF_ERROR(Set(XPRS_LPFLAGS, 1 << 0));
          break;
        case LP_ALGORITHM_BARRIER:
          RETURN_IF_ERROR(Set(XPRS_LPFLAGS, 1 << 2));
          break;
        case LP_ALGORITHM_FIRST_ORDER:
          RETURN_IF_ERROR(Set(XPRS_LPFLAGS, 1 << 2));
          RETURN_IF_ERROR(Set(XPRS_BARALG, 4));
          break;
          // Note: Xpress also supports network simplex, but that is not
          //       supported by ortools.
      }
    }
    if (parameters.presolve() != EMPHASIS_UNSPECIFIED) {
      // default value for XPRS_PRESOLVEPASSES is 1
      int presolvePasses = -1;
      switch (parameters.presolve()) {
        case EMPHASIS_OFF:
          RETURN_IF_ERROR(Set(XPRS_PRESOLVE, 0));  // Turn presolve off
          break;
        case EMPHASIS_LOW:
          presolvePasses = 2;
          break;
        case EMPHASIS_MEDIUM:
          presolvePasses = 3;
          break;
        case EMPHASIS_HIGH:
          presolvePasses = 4;
          break;
        case EMPHASIS_VERY_HIGH:
          presolvePasses = 5;
          break;
      }
      if (presolvePasses > 0)
        RETURN_IF_ERROR(Set(XPRS_PRESOLVEPASSES, presolvePasses));
    }
    if (parameters.cuts() != EMPHASIS_UNSPECIFIED) {
      switch (parameters.cuts()) {
        case EMPHASIS_OFF:
          RETURN_IF_ERROR(Set(XPRS_CUTSTRATEGY, 0));
          break;
        case EMPHASIS_LOW:
          RETURN_IF_ERROR(Set(XPRS_CUTSTRATEGY, 1));
          break;
        case EMPHASIS_MEDIUM:
          RETURN_IF_ERROR(Set(XPRS_CUTSTRATEGY, 2));
          break;
        case EMPHASIS_HIGH:
          RETURN_IF_ERROR(Set(XPRS_CUTSTRATEGY, 3));
          break;
        case EMPHASIS_VERY_HIGH:
          RETURN_IF_ERROR(Set(XPRS_CUTSTRATEGY, 3));  // Same as high
          break;
      }
    }
    if (parameters.heuristics() != EMPHASIS_UNSPECIFIED) {
      switch (parameters.heuristics()) {
        case EMPHASIS_OFF:
          break;
        case EMPHASIS_UNSPECIFIED:
          break;
        case EMPHASIS_LOW:  // fallthrough
        case EMPHASIS_MEDIUM:
          RETURN_IF_ERROR(Set(XPRS_HEUREMPHASIS, 1));
          break;
        case EMPHASIS_HIGH:  // fallthrough
        case EMPHASIS_VERY_HIGH:
          RETURN_IF_ERROR(Set(XPRS_HEUREMPHASIS, 2));
          break;
      }
    }

    for (const XpressParametersProto::Parameter& parameter :
         parameters.xpress().parameters()) {
      std::string const& name = parameter.name();
      std::string const& value = parameter.value();
      int id, type;
      int64_t l;
      double d;
      RETURN_IF_ERROR(xpress->GetControlInfo(name.c_str(), &id, &type));
      switch (type) {
        case XPRS_TYPE_INT:  // fallthrough
        case XPRS_TYPE_INT64:
          if (!absl::SimpleAtoi(value, &l))
            return util::StatusBuilder(absl::StatusCode::kInvalidArgument)
                   << "value " << value << " for " << name
                   << " is not an integer";
          if (type == XPRS_TYPE_INT && (l > std::numeric_limits<int>::max() ||
                                        l < std::numeric_limits<int>::min()))
            return util::StatusBuilder(absl::StatusCode::kInvalidArgument)
                   << "value " << value << " for " << name
                   << " is out of range";
          RETURN_IF_ERROR(Set(id, l));
          break;
        case XPRS_TYPE_DOUBLE:
          if (!absl::SimpleAtod(value, &d))
            return util::StatusBuilder(absl::StatusCode::kInvalidArgument)
                   << "value " << value << " for " << name
                   << " is not a floating pointer number";
          RETURN_IF_ERROR(Set(id, d));
          break;
        case XPRS_TYPE_STRING:
          RETURN_IF_ERROR(Set(id, value));
          break;
        default:
          return util::StatusBuilder(absl::StatusCode::kInvalidArgument)
                 << "bad control type for " << name;
      }
    }

    if (!warnings.empty()) {
      return absl::InvalidArgumentError(absl::StrJoin(warnings, "; "));
    }
    return absl::OkStatus();
  }
  /** TODO: Implement this (only for Solve(), not for
   *        ComputeInfeasibleSubsystem())
   * absl::Status ApplyParameters(const ModelSolveParametersProto&
   * model_parameters);
   */

  ~ScopedSolverContext() {
  ~SolveContext() {
    for (auto it = modifiedControls.rbegin(); it != modifiedControls.rend();
         ++it) {
      switch (it->type) {
        case OneControl::INT_CONTROL:
          CHECK_OK(xpress->SetIntControl64(it->id, it->l));
          break;
        case OneControl::DBL_CONTROL:
          CHECK_OK(xpress->SetDblControl(it->id, it->d));
          break;
        case OneControl::STR_CONTROL:
          CHECK_OK(xpress->SetStrControl(it->id, it->s.c_str()));
          break;
      }
    }
    if (removeInterrupterCallback) removeInterrupterCallback();
    // If pending callback exception was not reraised yet then do it now
    if (shared_ctx.callbackException)
      std::rethrow_exception(shared_ctx.callbackException);
  }

  absl::Status Set(int id, int32_t const& value) {
    return Set(id, int64_t(value));
  }
  absl::Status Set(int id, int64_t const& value) {
    ASSIGN_OR_RETURN(int64_t old, xpress->GetIntControl64(id));
    modifiedControls.push_back({OneControl::INT_CONTROL, id, old, 0.0, ""});
    RETURN_IF_ERROR(xpress->SetIntControl64(id, value));
    return absl::OkStatus();
  }
  absl::Status Set(int id, double const& value) {
    ASSIGN_OR_RETURN(double old, xpress->GetDblControl(id));
    modifiedControls.push_back({OneControl::DBL_CONTROL, id, 0LL, old, ""});
    RETURN_IF_ERROR(xpress->SetDblControl(id, value));
    return absl::OkStatus();
  }
  absl::Status Set(int id, std::string const& value) {
    ASSIGN_OR_RETURN(std::string old, xpress->GetStrControl(id));
    modifiedControls.push_back({OneControl::STR_CONTROL, id, 0LL, 0.0, old});
    RETURN_IF_ERROR(xpress->SetStrControl(id, value));
    return absl::OkStatus();
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
}  // namespace

constexpr SupportedProblemStructures kXpressSupportedStructures = {
    .integer_variables = SupportType::kSupported,
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
  bool have_integers = false;
  for (int j = 0; j < num_new_variables; ++j) {
    const VarId id = new_variables.ids(j);
    gtl::InsertOrDie(&variables_map_, id, j + n_variables);
    if (new_variables.integers(j)) {
      // Note: ortools does not distinguish between binary variables and
      //       integer variables in {0,1}
      variable_type[j] = XPRS_INTEGER;
      have_integers = true;
    } else {
      variable_type[j] = XPRS_CONTINUOUS;
    }
  }
  if (!have_integers) {
    // There are no integer variables, so we clear variable_type to
    // safe the call to XPRSchgcoltype() in AddVars()
    variable_type.clear();
  }
  RETURN_IF_ERROR(
      xpress_->AddVars(num_new_variables, {}, new_variables.lower_bounds(),
                       new_variables.upper_bounds(), variable_type));

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
    if (lb_is_xprs_neg_inf && ub_is_xprs_pos_inf) {
      // We have a row
      //   -inf <= expression <= inf
      // Xpress has no way to submit this as a ranged constraint. For Xpress
      // the upper bound of the constraint is just the ub and the lower bound
      // is computed as ub-abs(lb). This would result in inf-inf=nan if you
      // use IEEE infinity or XPRS_INFINITY - XPRS_INFINITY = 0. Both are wrong.
      // So we explicitly register this as free row.
      sense = XPRS_NONBINDING;
      rhs = 0.0;
      rng = 0.0;
    } else if (lb_is_xprs_neg_inf && !ub_is_xprs_pos_inf) {
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
  primal_sol_avail_ = XPRS_SOLAVAILABLE_NOTFOUND;
  dual_sol_avail_ = XPRS_SOLAVAILABLE_NOTFOUND;
  solvestatus_ = XPRS_SOLVESTATUS_UNSTARTED;
  solstatus_ = XPRS_SOLSTATUS_NOTFOUND;
  algorithm_ = XPRS_ALG_DEFAULT;
  RETURN_IF_ERROR(ModelSolveParametersAreSupported(
      model_parameters, kXpressSupportedStructures, "XPRESS"));
  ASSIGN_OR_RETURN(is_mip_, xpress_->IsMIP());
  const absl::Time start = absl::Now();

  RETURN_IF_ERROR(CheckRegisteredCallbackEvents(callback_registration,
                                                /*supported_events=*/{}));

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

  // Register callbacks and create scoped context to automatically if an
  // exception has been thrown during optimization.
  {
    ScopedSolverContext solveContext(xpress_.get());
    RETURN_IF_ERROR(solveContext.AddCallbacks(message_callback, interrupter));
    RETURN_IF_ERROR(solveContext.ApplyParameters(parameters, message_callback));
    // Solve. We use the generic XPRSoptimize() and let Xpress decide what is
    // the best algorithm. Note that we do not pass flags to the function either.
    // We assume that algorithms are configured via controls like LPFLAGS.

    RETURN_IF_ERROR(xpress_->Optimize("", &solvestatus_, &solstatus_));
  }  
  RETURN_IF_ERROR(xpress_->GetSolution(&primal_sol_avail_, nullptr, 0, -1));
  RETURN_IF_ERROR(xpress_->GetDuals(&dual_sol_avail_, nullptr, 0, -1));
  ASSIGN_OR_RETURN(algorithm_, xpress_->GetIntAttr(XPRS_ALGORITHM));
  RETURN_IF_ERROR(xpress_->PostSolve()) << "XPRSpostsolve() failed";

  ASSIGN_OR_RETURN(
      SolveResultProto solve_result,
      ExtractSolveResultProto(start, model_parameters, parameters));

  return solve_result;
}

absl::StatusOr<SolveResultProto> XpressSolver::ExtractSolveResultProto(
    absl::Time start, const ModelSolveParametersProto& model_parameters,
    const SolveParametersProto& solve_parameters) {
  SolveResultProto result;
  RETURN_IF_ERROR(AppendSolution(result, model_parameters, solve_parameters));
  ASSIGN_OR_RETURN(*result.mutable_solve_stats(), GetSolveStats(start));
  ASSIGN_OR_RETURN(const double best_primal_bound, GetBestPrimalBound());
  ASSIGN_OR_RETURN(const double best_dual_bound, GetBestDualBound());
  ASSIGN_OR_RETURN(
      *result.mutable_termination(),
      ConvertTerminationReason(best_primal_bound, best_dual_bound));
  return result;
}

absl::StatusOr<double> XpressSolver::GetBestPrimalBound() const {
  if (is_mip_) {
    return xpress_->GetDoubleAttr(XPRS_OBJVAL);
  } else if (primal_sol_avail_ == XPRS_SOLAVAILABLE_OPTIMAL ||
             primal_sol_avail_ == XPRS_SOLAVAILABLE_FEASIBLE) {
    return xpress_->GetDoubleAttr(XPRS_OBJVAL);
  }
  // No primal bound available, return infinity.
  ASSIGN_OR_RETURN(double const objsen, xpress_->GetDoubleAttr(XPRS_OBJSENSE));
  return objsen * kPlusInf;
}

absl::StatusOr<double> XpressSolver::GetBestDualBound() const {
  if (is_mip_) {
    return xpress_->GetDoubleAttr(XPRS_BESTBOUND);
  }
  // Xpress does not have an attribute to report the best dual bound from
  // simplex
  else {
    ASSIGN_OR_RETURN(int const alg, xpress_->GetIntAttr(XPRS_ALGORITHM));
    if (alg == XPRS_ALG_BARRIER)
      return xpress_->GetDoubleAttr(XPRS_BARDUALOBJ);
    else if (primal_sol_avail_ == XPRS_SOLAVAILABLE_OPTIMAL)
      return xpress_->GetDoubleAttr(XPRS_OBJVAL);
  }
  // No dual bound available, return infinity.
  ASSIGN_OR_RETURN(double const objsen, xpress_->GetDoubleAttr(XPRS_OBJSENSE));
  return objsen * kMinusInf;
}

absl::Status XpressSolver::AppendSolution(
    SolveResultProto& solve_result,
    const ModelSolveParametersProto& model_parameters,
    const SolveParametersProto& solve_parameters) {
  int const nVars = xpress_->GetNumberOfVariables();
  if (is_mip_) {
    std::vector<double> x(nVars);
    int avail;
    RETURN_IF_ERROR(xpress_->GetSolution(&avail, x.data(), 0, nVars - 1));
    if (avail != XPRS_SOLAVAILABLE_NOTFOUND) {
      SolutionProto solution{};
      solution.mutable_primal_solution()->set_feasibility_status(
          getPrimalSolutionStatus());
      ASSIGN_OR_RETURN(const double objval,
                       xpress_->GetDoubleAttr(XPRS_OBJVAL));
      solution.mutable_primal_solution()->set_objective_value(objval);
      XpressVectorToSparseDoubleVector(
          x, variables_map_,
          *solution.mutable_primal_solution()->mutable_variable_values(),
          model_parameters.variable_values_filter());
      *solve_result.add_solutions() = std::move(solution);
    }
  } else {
    // Fetch all results from XPRESS
    int const nCons = xpress_->GetNumberOfConstraints();
    std::vector<double> primals(nVars);
    std::vector<double> duals(nCons);
    std::vector<double> reducedCosts(nVars);

    auto hasSolution =
        xpress_
            ->GetLpSol(absl::MakeSpan(primals), absl::MakeSpan(duals),
                       absl::MakeSpan(reducedCosts))
            .ok();

    SolutionProto solution{};
    bool storeSolutions = (solvestatus_ == XPRS_SOLVESTATUS_STOPPED ||
                           solvestatus_ == XPRS_SOLVESTATUS_COMPLETED);

    if (isPrimalFeasible()) {
      // The preferred methods for obtaining primal information are
      // XPRSgetsolution() and XPRSgetslacks() (not used here)
      RETURN_IF_ERROR(
          xpress_->GetSolution(nullptr, primals.data(), 0, nVars - 1));
      solution.mutable_primal_solution()->set_feasibility_status(
          getPrimalSolutionStatus());
      ASSIGN_OR_RETURN(const double primalBound, GetBestPrimalBound());
      solution.mutable_primal_solution()->set_objective_value(primalBound);
      XpressVectorToSparseDoubleVector(
          primals, variables_map_,
          *solution.mutable_primal_solution()->mutable_variable_values(),
          model_parameters.variable_values_filter());
    } else if (storeSolutions) {
      // Even if we are not primal feasible, store the results we obtained
      // from XPRSgetlpsolution(). The feasibility status of this vector
      // is undetermined, though.
      solution.mutable_primal_solution()->set_feasibility_status(
          SOLUTION_STATUS_UNDETERMINED);
      ASSIGN_OR_RETURN(const double primalBound, GetBestPrimalBound());
      solution.mutable_primal_solution()->set_objective_value(primalBound);
      XpressVectorToSparseDoubleVector(
          primals, variables_map_,
          *solution.mutable_primal_solution()->mutable_variable_values(),
          model_parameters.variable_values_filter());
    }

    if (isDualFeasible()) {
      // The preferred methods for obtain dual information are XPRSgetduals()
      // and XPRSgetredcosts().
      RETURN_IF_ERROR(xpress_->GetDuals(nullptr, duals.data(), 0, nCons - 1));
      RETURN_IF_ERROR(
          xpress_->GetRedCosts(nullptr, reducedCosts.data(), 0, nVars - 1));
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
    } else if (storeSolutions) {
      // Even if we are not dual feasible, store the results we obtained from
      // XPRSgetlpsolution(). The feasibility status of this vector
      // is undetermined, though.
      solution.mutable_dual_solution()->set_feasibility_status(
          SOLUTION_STATUS_UNDETERMINED);
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
    *solve_result.add_solutions() = std::move(solution);
  }
  return absl::OkStatus();
}

bool XpressSolver::isPrimalFeasible() const {
  return primal_sol_avail_ == XPRS_SOLAVAILABLE_FEASIBLE ||
         primal_sol_avail_ == XPRS_SOLAVAILABLE_OPTIMAL;
}

bool XpressSolver::isDualFeasible() const {
  /** TODO: For MIP, should we return true if we are optimal? */
  return dual_sol_avail_ == XPRS_SOLAVAILABLE_FEASIBLE ||
         dual_sol_avail_ == XPRS_SOLAVAILABLE_OPTIMAL;
}

SolutionStatusProto XpressSolver::getPrimalSolutionStatus() const {
  switch (solvestatus_) {
    case XPRS_SOLVESTATUS_UNSTARTED:
      return SOLUTION_STATUS_UNDETERMINED;
    case XPRS_SOLVESTATUS_STOPPED:    // fallthrough
    case XPRS_SOLVESTATUS_FAILED:     // fallthrough
    case XPRS_SOLVESTATUS_COMPLETED:  // fallthrough
      break;
  }
  switch (solstatus_) {
    case XPRS_SOLSTATUS_NOTFOUND:
      return SOLUTION_STATUS_UNDETERMINED;
    case XPRS_SOLSTATUS_OPTIMAL:  // fallthrough
    case XPRS_SOLSTATUS_FEASIBLE:
      return SOLUTION_STATUS_FEASIBLE;
    case XPRS_SOLSTATUS_INFEASIBLE:
      return SOLUTION_STATUS_INFEASIBLE;
    case XPRS_SOLSTATUS_UNBOUNDED:
      return SOLUTION_STATUS_UNDETERMINED;
  }
  return SOLUTION_STATUS_UNSPECIFIED;
}

SolutionStatusProto XpressSolver::getDualSolutionStatus() const {
  if (dual_sol_avail_ == XPRS_SOLAVAILABLE_OPTIMAL ||
      dual_sol_avail_ == XPRS_SOLAVAILABLE_FEASIBLE)
    return SOLUTION_STATUS_FEASIBLE;
  /** TODO: Should we be more specific here? If primal is unbounded we
   *        know that dual is infeasible.
   */
  return SOLUTION_STATUS_UNDETERMINED;
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

  int simplex_iters = 0;
  int barrier_iters = 0;
  int first_order_iters = 0;
  if (algorithm_ == XPRS_ALG_DEFAULT) {
    // Could be concurrent, so capture simplex and barrier iterations
    ASSIGN_OR_RETURN(simplex_iters, xpress_->GetIntAttr(XPRS_SIMPLEXITER));
    ASSIGN_OR_RETURN(barrier_iters, xpress_->GetIntAttr(XPRS_BARITER));
  } else if (algorithm_ == XPRS_ALG_DUAL || algorithm_ == XPRS_ALG_PRIMAL ||
             algorithm_ == XPRS_ALG_NETWORK) {
    // Definitely simplex
    ASSIGN_OR_RETURN(simplex_iters, xpress_->GetIntAttr(XPRS_SIMPLEXITER));
  } else if (algorithm_ == XPRS_ALG_BARRIER) {
    // Barrier or first order
    ASSIGN_OR_RETURN(const int baralg, xpress_->GetIntControl(XPRS_BARALG));
    if (baralg == 4) {
      ASSIGN_OR_RETURN(first_order_iters, xpress_->GetIntAttr(XPRS_BARITER));
    } else {
      ASSIGN_OR_RETURN(barrier_iters, xpress_->GetIntAttr(XPRS_BARITER));
    }
  }
  solve_stats.set_simplex_iterations(simplex_iters);
  solve_stats.set_barrier_iterations(barrier_iters);
  solve_stats.set_first_order_iterations(first_order_iters);
  if (is_mip_) {
    ASSIGN_OR_RETURN(const int nodes, xpress_->GetIntAttr(XPRS_NODES));
    solve_stats.set_node_count(nodes);
  }
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
  ASSIGN_OR_RETURN(double const objsen, xpress_->GetDoubleAttr(XPRS_OBJSENSE));
  ASSIGN_OR_RETURN(int const stopStatus, xpress_->GetIntAttr(XPRS_STOPSTATUS));
  bool const isMax = objsen < 0.0;
  bool checkSolStatus = false;

  if (!is_mip_) {
    // Handle some special LP termination reasons.
    ASSIGN_OR_RETURN(int const lpstatus, xpress_->GetIntAttr(XPRS_LPSTATUS));
    switch (lpstatus) {
      case XPRS_LP_UNSTARTED:
        break;
      case XPRS_LP_OPTIMAL:
        break;
      case XPRS_LP_INFEAS:
        break;
      case XPRS_LP_CUTOFF:
        // This can happen if you set MIPABSCUTOFF for an LP
        return NoSolutionFoundTerminationProto(
            isMax, LIMIT_CUTOFF, std::nullopt, "Objective limit (LP_CUTOFF)");
        break;
      case XPRS_LP_UNFINISHED:
        break;
      case XPRS_LP_UNBOUNDED:
        break;
      case XPRS_LP_CUTOFF_IN_DUAL:
        // This can happen if you set MIPABSCUTOFF for an LP
        return NoSolutionFoundTerminationProto(
            isMax, LIMIT_CUTOFF, std::nullopt,
            "Objective limit (LP_CUTOFF_IN_DUAL)");
      case XPRS_LP_UNSOLVED:
        break;
      case XPRS_LP_NONCONVEX:
        return TerminateForReason(isMax, TERMINATION_REASON_OTHER_ERROR,
                                  "Problem contains quadratic data, which is "
                                  "not convex (XPRS_LP_NONCONVEX)");
    }
  }

  // First check how far the solve actually got.
  switch (solvestatus_) {
    case XPRS_SOLVESTATUS_UNSTARTED:
      return TerminateForReason(isMax, TERMINATION_REASON_OTHER_ERROR,
                                "Problem solve has not started");
      break;
    case XPRS_SOLVESTATUS_STOPPED:
      checkSolStatus = true;
      break;
    case XPRS_SOLVESTATUS_FAILED:
      switch (stopStatus) {
        case XPRS_STOP_GENERICERROR:
          return TerminateForReason(isMax, TERMINATION_REASON_OTHER_ERROR,
                                    "Generic error");
        case XPRS_STOP_MEMORYERROR:
          // This can actually not happen since despite its name, this is
          // not an error but indicates hitting a user defined memory limit
          return TerminateForReason(isMax, TERMINATION_REASON_OTHER_ERROR,
                                    "Memory error");
        case XPRS_STOP_LICENSELOST:
          return TerminateForReason(isMax, TERMINATION_REASON_OTHER_ERROR,
                                    "License lost");
        case XPRS_STOP_NUMERICALERROR:
          return TerminateForReason(isMax, TERMINATION_REASON_NUMERICAL_ERROR,
                                    "Numerical issues");
        default:
          return TerminateForReason(isMax, TERMINATION_REASON_OTHER_ERROR,
                                    "Problem solve failed");
      }
      break;
    case XPRS_SOLVESTATUS_COMPLETED:
      checkSolStatus = true;
      break;
  }
  if (checkSolStatus) {
    // Algorithm finished or was stopped on purpose
    switch (solstatus_) {
      case XPRS_SOLSTATUS_NOTFOUND:
        switch (stopStatus) {
          case XPRS_STOP_TIMELIMIT:
            return NoSolutionFoundTerminationProto(
                isMax, LIMIT_TIME, std::nullopt, /** TODO: bound? */
                "Time limit hit");
          case XPRS_STOP_CTRLC:  // fallthrough
          case XPRS_STOP_USER:
            return NoSolutionFoundTerminationProto(
                isMax, LIMIT_INTERRUPTED, std::nullopt, /** TODO: bound? */
                "Interrupted");
          case XPRS_STOP_NODELIMIT:
            return NoSolutionFoundTerminationProto(
                isMax, LIMIT_NODE, std::nullopt, /** TODO: bound? */
                "Node limit hit");
          case XPRS_STOP_ITERLIMIT:
            return NoSolutionFoundTerminationProto(
                isMax, LIMIT_ITERATION, std::nullopt, /** TODO: bound? */
                "Node limit hit");
          case XPRS_STOP_WORKLIMIT:
            return NoSolutionFoundTerminationProto(
                isMax, LIMIT_OTHER, std::nullopt, /** TODO: bound? */
                "Work limit hit");
          case XPRS_STOP_MEMORYERROR:
            // Despite its name, MEMORYERROR is not actually an error
            // but instead indicates that we hit a user defined memory
            // limit.
            return NoSolutionFoundTerminationProto(
                isMax, LIMIT_MEMORY, std::nullopt, /** TODO: bound? */
                "Memory limit hit");
          default:
            return TerminateForReason(isMax,
                                      TERMINATION_REASON_NO_SOLUTION_FOUND);
        }
        break;
      case XPRS_SOLSTATUS_OPTIMAL:
        return OptimalTerminationProto(best_primal_bound, best_dual_bound);
        break;
      case XPRS_SOLSTATUS_FEASIBLE:
        switch (stopStatus) {
          case XPRS_STOP_TIMELIMIT:
            return FeasibleTerminationProto(isMax, LIMIT_TIME,
                                            best_primal_bound, best_dual_bound,
                                            "Time limit hit");
          case XPRS_STOP_CTRLC:  // fallthrough
          case XPRS_STOP_USER:
            return FeasibleTerminationProto(isMax, LIMIT_INTERRUPTED,
                                            best_primal_bound, best_dual_bound,
                                            "Interrupted");
          case XPRS_STOP_NODELIMIT:
            return FeasibleTerminationProto(isMax, LIMIT_NODE,
                                            best_primal_bound, best_dual_bound,
                                            "Node limit hit");
          case XPRS_STOP_ITERLIMIT:
            return FeasibleTerminationProto(isMax, LIMIT_ITERATION,
                                            best_primal_bound, best_dual_bound,
                                            "Node limit hit");
          case XPRS_STOP_WORKLIMIT:
            return FeasibleTerminationProto(isMax, LIMIT_OTHER,
                                            best_primal_bound, best_dual_bound,
                                            "Work limit hit");
          case XPRS_STOP_MEMORYERROR:
            // Despite its name, MEMORYERROR is not actually an error
            // but instead indicates that we hit a user defined memory
            // limit.
            return FeasibleTerminationProto(isMax, LIMIT_MEMORY,
                                            best_primal_bound, best_dual_bound,
                                            "Memory limit hit");
          default:
            return FeasibleTerminationProto(isMax, LIMIT_UNDETERMINED,
                                            best_primal_bound, best_dual_bound);
        }
        break;
      case XPRS_SOLSTATUS_INFEASIBLE:
        return InfeasibleTerminationProto(
            isMax, isDualFeasible() ? FEASIBILITY_STATUS_FEASIBLE
                                    : FEASIBILITY_STATUS_UNDETERMINED);
      case XPRS_SOLSTATUS_UNBOUNDED:
        return UnboundedTerminationProto(isMax);
    }
  }
  return TerminateForReason(isMax, TERMINATION_REASON_OTHER_ERROR,
                            "Unknown error");
}

absl::StatusOr<bool> XpressSolver::Update(const ModelUpdateProto&) {
  // Not implemented yet
  return false;
}

absl::StatusOr<ComputeInfeasibleSubsystemResultProto>
XpressSolver::ComputeInfeasibleSubsystem(const SolveParametersProto& parameters,
                                         MessageCallback message_callback,
                                         const SolveInterrupter* interrupter) {
  ScopedSolverContext solveContext(xpress_.get());
  RETURN_IF_ERROR(solveContext.AddCallbacks(message_callback, interrupter));
  RETURN_IF_ERROR(solveContext.ApplyParameters(parameters, message_callback));

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
