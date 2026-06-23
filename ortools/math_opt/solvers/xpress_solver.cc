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

#include "absl/container/linked_hash_map.h"
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
#include "ortools/math_opt/cpp/math_opt.h"
#include "ortools/math_opt/cpp/streamable_solver_init_arguments.h"
#include "ortools/math_opt/solvers/xpress/g_xpress.h"
#include "ortools/math_opt/validators/callback_validator.h"
#include "ortools/port/proto_utils.h"
#include "ortools/third_party_solvers/xpress_environment.h"
#include "ortools/util/solve_interrupter.h"

namespace operations_research {
namespace math_opt {

// The callback events that Xpress supports.
// For Xpress it is not a problem to listen to MIP events when solving an LP,
// they will just never be triggered. However, the testsuite expects that we
// raise an error if someone attempts to register MIP events for a non-MIP
// solve, so we have to support different callbacks depending on the problem
// type.

absl::flat_hash_set<CallbackEventProto> const XpressSolver::SupportedMIPEvents_(
    {
        CALLBACK_EVENT_PRESOLVE,
        CALLBACK_EVENT_SIMPLEX,
        CALLBACK_EVENT_MIP,
        CALLBACK_EVENT_MIP_SOLUTION,
        CALLBACK_EVENT_MIP_NODE,
        CALLBACK_EVENT_BARRIER,
    });
absl::flat_hash_set<CallbackEventProto> const XpressSolver::SupportedLPEvents_({
    CALLBACK_EVENT_PRESOLVE,
    CALLBACK_EVENT_SIMPLEX,
    CALLBACK_EVENT_BARRIER,
});

namespace {

/** Map an ortools variable or objective id to an Xpress column index.
 * This will raise an exception if the ortools id does not exist in the map.
 */
int or2xprs(absl::linked_hash_map<int64_t, int> const& varMap, int64_t orId) {
  return varMap.at(orId);  // raises exception if no such key
}

/** Map an ortools constraint id to an Xpress row index.
 * This will raise an exception if the ortools id does not exist in the map.
 */
XpressSolver::XpressLinearConstraintIndex or2xprs(
    absl::linked_hash_map<XpressSolver::LinearConstraintId,
                          XpressSolver::LinearConstraintData> const& conMap,
    XpressSolver::LinearConstraintId orId) {
  return conMap.at(orId).constraint_index;  // raises exception if no such key
}

class SharedSolveContext {
  /** Mutex for accessing callbackException or callbackStatus. */
  absl::Mutex mutex;

  /** Capturing of exceptions in callbacks.
   * We cannot let exceptions escape from callbacks since that would just
   * unroll the stack until some function that catches the exception.
   * In particular, it would bypass any cleanup code implemented in the C code
   * of the solver. So we must capture exceptions, interrupt the solve and
   * handle the exception once the solver returned.
   */
  std::exception_ptr callbackException;

  /** Capturing of errors in callbacks.
   * This allows us to report errors that occur in a callback back to
   * the user once the solve is complete.
   */
  absl::Status callbackStatus;

 public:
  /** The Xpress instance we use for adding and removing callbacks,
   * for querying attributes and setting controls, etc.
   */
  Xpress* const xpress;

  SharedSolveContext(Xpress* xprs)
      : callbackException(nullptr),
        callbackStatus(absl::OkStatus()),
        xpress(xprs) {}

  ~SharedSolveContext() {
    // If pending callback exception was not re-raised yet then do it now.
    // Raising exceptions from a destructor is usually a bad idea. We do it
    // only for RAII purposes and as the very last thing in the destructor.
    // Note that instances of this class are only ever allocated on the stack,
    // so an exception will not bypass memory deallocation.
    if (callbackException) std::rethrow_exception(callbackException);
  }

  inline void SetCallbackException(std::exception_ptr ex) {
    absl::MutexLock const lock(mutex);
    if (!callbackException) callbackException = ex;
  }

  inline void SetCallbackStatus(absl::Status const& status) {
    absl::MutexLock const lock(mutex);
    if (callbackStatus.ok()) callbackStatus = status;
  }

  /** Handle errors that occured during callbacks.
   * This is supposed to be called after a solve completes. If there was an
   * exception in a callback then this function re-raises the exception.
   * If there was an error during any callback then this function returns that
   * error.
   */
  absl::Status HandleCallbackProblems() {
    // If callbacks raised an exception then re-raise that now.
    if (callbackException) {
      std::rethrow_exception(std::exchange(callbackException, nullptr));
    }
    // If callbacks produced an error then return that now.
    return callbackStatus;
  }
};

/** Base class for scoped callbacks.
 * This provides everything the ScopedCallback class requires that does not
 * depend on template arguments.
 */
class ScopedCallbackBase {
  ScopedCallbackBase(ScopedCallbackBase const&) = delete;
  ScopedCallbackBase(ScopedCallbackBase&&) = delete;
  ScopedCallbackBase& operator=(ScopedCallbackBase const&) = delete;
  ScopedCallbackBase& operator=(ScopedCallbackBase&&) = delete;

 protected:
  SharedSolveContext* ctx;
  ScopedCallbackBase() : ctx(nullptr) {}

 public:
  /** Store an exception that case raised during a callback.
   * Only the first such exception will be remembered.
   */
  inline void SetCallbackException(std::exception_ptr ex) {
    ctx->SetCallbackException(ex);
  }

  /** Store an error status that occurred during a callback.
   * Only the first such error will be remembered.
   */
  inline void SetCallbackStatus(absl::Status const& status) {
    ctx->SetCallbackStatus(status);
  }
};

/** Registered callback that is auto-removed in the destructor.
 * Use Add() to add a callback to a solve context.
 */
template <typename ProtoT, typename CbT>
class ScopedCallback : public ScopedCallbackBase {
  using proto_type = typename ProtoT::proto_type;

  // We intercept and store any exception throw by a callback defining a static
  // wrapper function that invokes the callback within a try/carch block. For
  // this to work, we need to deduce the callback return type and arguments.
  template <typename FuncPtr>
  struct ExWrapper;

  // Specialization to deduce the callback return and arguments types
  template <typename R, typename... Args>
  struct ExWrapper<R (*)(XPRSprob, void*, Args...)> {
    // The static function that will be directly invoked by Xpress
    // Note: Xpress callbacks return either void or int. All callbacks
    //       that return int treat zero as "continue" and non-zero as "stop"
    //       (either explicit stop request or error).
    static R low_level_cb(XPRSprob prob, void* cbdata, Args... args) {
      ScopedCallback* cb = reinterpret_cast<ScopedCallback*>(cbdata);
      try {
        absl::Status status = ProtoT::glueFn(prob, cbdata, args...);
        if (status.ok()) return static_cast<R>(0);  // void-cast ignores value
        XPRSinterrupt(prob, XPRS_STOP_GENERICERROR);
        cb->SetCallbackStatus(status);
      } catch (...) {
        // Catch any exception and terminate Xpress gracefully
        XPRSinterrupt(prob, XPRS_STOP_USER);
        cb->SetCallbackException(std::current_exception());
      }
      // We get here only if an error or an exception occurred.
      return static_cast<R>(1);  // void-cast ignores value
    }
  };
  const proto_type low_level_cb = ExWrapper<proto_type>::low_level_cb;

 public:
  CbT or_tools_cb;

  ScopedCallback() : ScopedCallbackBase() {}

  inline absl::Status Add(SharedSolveContext* context, CbT cb, int prio = 0) {
    ctx = context;
    OR_RETURN_IF_ERROR(ProtoT::Add(ctx->xpress, low_level_cb,
                                   reinterpret_cast<void*>(this), prio));
    or_tools_cb = cb;
    return absl::OkStatus();
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
#define DEFINE_SCOPED_CB(CB_NAME, ORTOOLS_CB, CB_RET_TYPE, ARGS)           \
  absl::Status CB_NAME##GlueFn ARGS;                                       \
  struct CB_NAME##Traits {                                                 \
    using proto_type = CB_RET_TYPE(XPRS_CC*) ARGS;                         \
    static constexpr absl::Status(XPRS_CC* glueFn) ARGS = CB_NAME##GlueFn; \
    static absl::Status Add(Xpress* xpress, proto_type fn, void* data,     \
                            int prio = 0) {                                \
      return xpress->AddCb##CB_NAME(fn, data, prio);                       \
    }                                                                      \
    static void Remove(Xpress* xpress, proto_type fn, void* data) {        \
      CHECK_OK(xpress->RemoveCb##CB_NAME(fn, data));                       \
    }                                                                      \
  };                                                                       \
  using CB_NAME##ScopedCb = ScopedCallback<CB_NAME##Traits, ORTOOLS_CB>;   \
  absl::Status CB_NAME##GlueFn ARGS

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
    return absl::OkStatus();
  }

  if (len == 0) {
    cb->or_tools_cb(std::vector<std::string>{""});
    return absl::OkStatus();
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
  return absl::OkStatus();
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
  //       the resulting stop status to ortools' termination status.
  if (cb->or_tools_cb->IsInterrupted()) {
    OR_RETURN_IF_ERROR(
        Xpress::ToStatus(prob, XPRSinterrupt(prob, XPRS_STOP_USER)));
  }
  return absl::OkStatus();
}

/** This is passed as user data to the callback. */
struct OrtoolsCallbackContext {
  /** Storage for solutions that cannot be injected in the callback in which
   * the user returns them.
   * This is needed since not all Xpress callbacks allow injection of solutions
   * in all situations.
   */
  struct SolStore {
    std::vector<int> ind;
    std::vector<double> val;
  };
  /** Storage for cuts or lazy constraints that cannot be injected in the
   * callback in which the user returns them.
   * This is needed since not all Xpress callbacks allow injection of cuts or
   * lazy constraints in all situations.
   */
  struct CutStore {
    std::vector<XPRSint64> start;
    std::vector<int> ind;
    std::vector<double> val;
    std::vector<char> sense;
    std::vector<double> rhs;

    /** Add the cuts stored in this instance as managed cuts to prob.
     * The function assumes that all cuts stored in this instance are stated
     * in the original space.
     */
    absl::Status AddManagedCuts(XPRSprob prob) {
      if (start.empty()) return absl::OkStatus();
      constexpr int const globallyValid =
          1;  // Cuts must always be globally valid.
      start.push_back(ind.size());
      int const xprs_err = XPRSaddmanagedcuts64(
          prob, globallyValid, rhs.size(), sense.data(), rhs.data(),
          start.data(), ind.data(), val.data());
      start.clear();
      ind.clear();
      val.clear();
      sense.clear();
      rhs.clear();
      return Xpress::ToStatus(prob, xprs_err);
    }

    /** Add the cuts stored in this instance as lazy constraints to prob.
     * The function assumes that the cuts stored are already presolved.
     */
    absl::Status AddLazyConstraints(XPRSprob prob) {
      if (start.empty()) return absl::OkStatus();
      start.push_back(ind.size());
      std::vector<int> cuttype(rhs.size());  // Cannot be null.
      int const xprs_err =
          XPRSaddcuts64(prob, rhs.size(), cuttype.data(), sense.data(),
                        rhs.data(), start.data(), ind.data(), val.data());
      start.clear();
      ind.clear();
      val.clear();
      sense.clear();
      rhs.clear();
      return Xpress::ToStatus(prob, xprs_err);
    }
  };
  /** ortools callback function. */
  SolverInterface::Callback const cb_;
  /** Maps ortools variable ids to Xpress column indices in the original
   * space. This is a reference to XpressSolver::variables_map_.
   */
  absl::linked_hash_map<int64_t, int> const& varMap_;
  /** The solution filter specified by the user for CALLBACK_EVENT_MIP_SOLUTION.
   */
  SparseVectorFilterProto const& mip_solution_filter_;
  /** The solution filter specified by the user for CALLBACK_EVENT_MIP_NODE. */
  SparseVectorFilterProto const& mip_node_filter_;

 private:
  /** Mutex for accessing the maps below. */
  absl::Mutex mutex;
  /** Lazy constraints that could not be injected at the time they were
   * separated. */
  absl::linked_hash_map<int, CutStore> delayedLazyConstraints_;
  /** User cuts that could not be injected at the time they were separated. */
  absl::linked_hash_map<int, CutStore> delayedCuts_;
  /** Feasible solutions that could not be injected at the time they were
   * provided. */
  absl::linked_hash_map<int, std::vector<SolStore>> delayedSols_;
  /** The time at which the solve started. */
  absl::Time const startTime_;

 public:
  OrtoolsCallbackContext(SolverInterface::Callback const& cb,
                         CallbackRegistrationProto const& callback_registration,
                         absl::linked_hash_map<int64_t, int> const& varMap)
      : cb_(cb),
        varMap_(varMap),
        mip_solution_filter_(callback_registration.mip_solution_filter()),
        mip_node_filter_(callback_registration.mip_node_filter()),
        startTime_(absl::Now()) {}

  template <typename ElemT>
  ElemT* GetDelayedEntity_(int threadID, bool create,
                           absl::linked_hash_map<int, ElemT>& map) {
    absl::MutexLock const lock(mutex);
    if (!create) {
      auto it = map.find(threadID);
      return it != map.end() ? &it->second : nullptr;
    }
    auto res = map.try_emplace(threadID);
    return &res.first->second;
  }

 public:
  /** Get the store for delayed lazy constraints for the specified thread.
   * @param threadID  The thread for which we should get the store.
   * @param create    If true then the store will be created if it does not
   *                  yet exist.
   * @return The requested store or nullptr if that does not exist and
   *         create was false.
   */
  CutStore* GetDelayedLazyConstraints(int threadID, bool create) {
    return GetDelayedEntity_(threadID, create, delayedLazyConstraints_);
  }

  /** Get the store for delayed cuts for the specified thread.
   * @param threadID  The thread for which we should get the store.
   * @param create    If true then the store will be created if it does not
   *                  yet exist.
   * @return The requested store or nullptr if that does not exist and
   *         create was false.
   */
  CutStore* GetDelayedCuts(int threadID, bool create) {
    return GetDelayedEntity_(threadID, create, delayedCuts_);
  }

  /** Get the store for delayed solutions for the specified thread.
   * @param threadID  The thread for which we should get the store.
   * @param create    If true then the store will be created if it does not
   *                  yet exist.
   * @return The requested store or nullptr if that does not exist and
   *         create was false.
   */
  std::vector<SolStore>* GetDelayedSolutions(int threadID, bool create) {
    return GetDelayedEntity_(threadID, create, delayedSols_);
  }

  /** Flush any delayed info from a callback for the current thread.
   * @param prob       The problem instance that was passed into the callback.
   * @param flushCuts  Whether we are allowed to flush user cuts.
   */
  absl::Status FlushDelayedInfo(XPRSprob prob, bool flushCuts) {
    int threadID = -1;
    OR_RETURN_IF_ERROR(Xpress::ToStatus(
        prob, XPRSgetintattrib(prob, XPRS_MIPTHREADID, &threadID)));

    CutStore* store = nullptr;
    if (flushCuts) {
      if ((store = GetDelayedCuts(threadID, false))) {
        OR_RETURN_IF_ERROR(store->AddManagedCuts(prob));
      }
    }
    if ((store = GetDelayedLazyConstraints(threadID, false))) {
      OR_RETURN_IF_ERROR(store->AddLazyConstraints(prob));
    }
    std::vector<SolStore>* sols = GetDelayedSolutions(threadID, false);
    if (sols) {
      int xprs_err = 0;
      for (auto const& s : *sols) {
        xprs_err = XPRSaddmipsol(prob, s.ind.size(), s.val.data(), s.ind.data(),
                                 nullptr);
        if (xprs_err != 0) break;
      }
      sols->clear();
      OR_RETURN_IF_ERROR(Xpress::ToStatus(prob, xprs_err));
    }
    return absl::OkStatus();
  }

  /** Set the elapsed time in callback data.
   */
  absl::Status SetElapsed(XPRSprob prob, CallbackDataProto& cbdata) {
    OR_RETURN_IF_ERROR(util_time::EncodeGoogleApiProto(
        absl::Now() - startTime_, cbdata.mutable_runtime()));
    return absl::OkStatus();
  }

  /** Apply a solutionn filter to reduce the number of non-zeros in a
   * solution.
   */
  SparseDoubleVectorProto FilterSolution(
      absl::Span<const double> dense, const SparseVectorFilterProto& filter) {
    SparseVectorFilterPredicate predicate(filter);
    SparseDoubleVectorProto result;
    for (auto const [id, idx] : varMap_) {
      const double val = dense[idx];
      if (predicate.AcceptsAndUpdate(id, val)) {
        result.add_ids(id);
        result.add_values(val);
      }
    }
    return result;
  }
};

// Query attributes from callbacks.
// Since we do not want to create an Xpress instance for every callback
// invocation, we just directly call XPRSgetintattrib() and friends from
// callbacks.
absl::Status GetAttr(XPRSprob prob, int attr, int* value) {
  return Xpress::ToStatus(prob, XPRSgetintattrib(prob, attr, value));
}
absl::Status GetAttr(XPRSprob prob, int attr, int64_t* value) {
  XPRSint64 xval;
  int err = XPRSgetintattrib64(prob, attr, &xval);
  *value = xval;
  return Xpress::ToStatus(prob, err);
}
absl::Status GetAttr(XPRSprob prob, int attr, double* value) {
  return Xpress::ToStatus(prob, XPRSgetdblattrib(prob, attr, value));
}

/** Set an attribute for a CallbackDataProto.
 * The macro assumes that  an XPRSprob `prob` is in the current scope.
 * @param stats  The field of CallbackDataProto to be set.
 * @param orattr The attribute to set in stats.
 * @param xattr  The XPRS_FOO attribute to query.
 */
#define CALLBACK_SET_ATTRIBUTE(stats, orattr, xattr)   \
  do {                                                 \
    decltype(stats->orattr()) value_;                  \
    OR_RETURN_IF_ERROR(GetAttr(prob, xattr, &value_)); \
    stats->set_##orattr(value_);                       \
  } while (0)

/** Initialize simplex statistics in cbdata.
 * @param prob    Problem passed into callback.
 * @param cbdata  Data that will be passed to the ortools callback.
 */
absl::Status InitSimplexStats(XPRSprob prob, CallbackDataProto& cbdata) {
  CallbackDataProto::SimplexStats* const s = cbdata.mutable_simplex_stats();
  CALLBACK_SET_ATTRIBUTE(s, iteration_count, XPRS_SIMPLEXITER);
  /* This is not available in Xpress
  CALLBACK_SET_ATTRIBUTE(s, is_perturbed, );
   */
  CALLBACK_SET_ATTRIBUTE(s, objective_value, XPRS_LPOBJVAL);
  CALLBACK_SET_ATTRIBUTE(s, primal_infeasibility, XPRS_SUMPRIMALINF);
  /* Xpress only has XPRS_SUMPRIMALINF, not XPRS_SUMDUALINF
  CALLBACK_SET_ATTRIBUTE(s, dual_infeasibility, );
   */
  return absl::OkStatus();
}

/** Initialize barrier statistics in cbdata.
 * @param prob    Problem passed into callback.
 * @param cbdata  Data that will be passed to the ortools callback.
 */
absl::Status InitBarrierStats(XPRSprob prob, CallbackDataProto& cbdata) {
  CallbackDataProto::BarrierStats* const s = cbdata.mutable_barrier_stats();

  CALLBACK_SET_ATTRIBUTE(s, iteration_count, XPRS_BARITER);
  CALLBACK_SET_ATTRIBUTE(s, primal_objective, XPRS_BARPRIMALOBJ);
  CALLBACK_SET_ATTRIBUTE(s, dual_objective, XPRS_BARDUALOBJ);
  CALLBACK_SET_ATTRIBUTE(s, complementarity, XPRS_BARCGAP);
  CALLBACK_SET_ATTRIBUTE(s, primal_infeasibility, XPRS_BARPRIMALINF);
  CALLBACK_SET_ATTRIBUTE(s, dual_infeasibility, XPRS_BARDUALINF);
  return absl::OkStatus();
}

/** Initialize presolve statistics in cbdata.
 * @param prob    Problem passed into callback.
 * @param cbdata  Data that will be passed to the ortools callback.
 */
absl::Status InitPresolveStats(XPRSprob prob, CallbackDataProto& cbdata) {
  int cols, origCols, rows, origRows;
  OR_RETURN_IF_ERROR(
      Xpress::ToStatus(prob, XPRSgetintattrib(prob, XPRS_COLS, &cols)));
  OR_RETURN_IF_ERROR(Xpress::ToStatus(
      prob, XPRSgetintattrib(prob, XPRS_ORIGINALCOLS, &origCols)));
  OR_RETURN_IF_ERROR(
      Xpress::ToStatus(prob, XPRSgetintattrib(prob, XPRS_ROWS, &rows)));
  OR_RETURN_IF_ERROR(Xpress::ToStatus(
      prob, XPRSgetintattrib(prob, XPRS_ORIGINALROWS, &origRows)));

  CallbackDataProto::PresolveStats* const s = cbdata.mutable_presolve_stats();
  s->set_removed_variables(origCols - cols);
  s->set_removed_constraints(origRows - rows);
  /* These two are not available in Xpress.
   s->set_bound_changes()
   s->set_coefficient_changes()
   */
  return absl::OkStatus();
}

/** Initialize MIP statistics in cbdata.
 * @param prob    Problem passed into callback.
 * @param cbdata  Data that will be passed to the ortools callback.
 */
absl::Status InitMipStats(XPRSprob prob, CallbackDataProto& cbdata) {
  CallbackDataProto::MipStats* const s = cbdata.mutable_mip_stats();

  CALLBACK_SET_ATTRIBUTE(s, primal_bound, XPRS_MIPBESTOBJVAL);
  CALLBACK_SET_ATTRIBUTE(s, dual_bound, XPRS_BESTBOUND);
  CALLBACK_SET_ATTRIBUTE(s, explored_nodes, XPRS_NODES);
  CALLBACK_SET_ATTRIBUTE(s, open_nodes, XPRS_ACTIVENODES);
  // Note that in multi-threading SIMPLEXITER gives the iterations per worker,
  // not the global grand total. That will only be reported after the solve.
  CALLBACK_SET_ATTRIBUTE(s, simplex_iterations, XPRS_SIMPLEXITER);
  CALLBACK_SET_ATTRIBUTE(s, number_of_solutions_found, XPRS_MIPSOLS);
  CALLBACK_SET_ATTRIBUTE(s, cutting_planes_in_lp, XPRS_CUTS);
  return absl::OkStatus();
}

/** Invoke the ortools callback.
 * The function also handles all actions requested by the callback, such as
 * termination requests, injected solutions, add cuts/lazy constraints.
 * @param ctx             Global callback context.
 * @param prob            The XPRSprob passed into the Xpress callback.
 * @param data            Data passed into the ortools callback.
 *                        Everything but the runtime field must be setup.
 * @param allowCuts       If this is true then the callback is allowed to
 *                        add cuts or lazy constraints.
 * @param modifiableNode  True if the current node can be modified, i.e.,
 *                        we can add solutions, cuts, lazy constraints.
 * @param hadLazy         Set to true if we had any lazy constraint.
 */
absl::Status InvokeOrtoolsCallback(OrtoolsCallbackContext* ctx, XPRSprob prob,
                                   CallbackDataProto& data, bool allowCuts,
                                   bool modifiableNode, bool* hadLazy) {
  OR_RETURN_IF_ERROR(ctx->SetElapsed(prob, data));
  OR_ASSIGN_OR_RETURN(CallbackResultProto result, ctx->cb_(data));
  // The variables below are needed to presolve lazy constraints. They are
  // only initialized when we have to presolve the first lazy constraint.
  int origCols = -1, cols = -1, threadID = -1;
  std::vector<int> origInd, preInd;
  std::vector<double> origVal, preVal;

  OR_RETURN_IF_ERROR(Xpress::ToStatus(
      prob, XPRSgetintattrib(prob, XPRS_MIPTHREADID, &threadID)));

  // Setup stores for cuts/lazy constraints, depending on whether we can
  // add them directly or need to cache them.
  OrtoolsCallbackContext::CutStore cutsToCommit;
  OrtoolsCallbackContext::CutStore* cutStore = &cutsToCommit;
  OrtoolsCallbackContext::CutStore* lazyStore = nullptr;
  std::vector<OrtoolsCallbackContext::SolStore>* solStore = nullptr;
  if (!modifiableNode) {
    cutStore = ctx->GetDelayedCuts(threadID, true);
    lazyStore = ctx->GetDelayedLazyConstraints(threadID, true);
    solStore = ctx->GetDelayedSolutions(threadID, true);
  }

  // Go through all linear constraints returned by the callback.
  for (CallbackResultProto::GeneratedLinearConstraint const& cut :
       result.cuts()) {
    if (!allowCuts) {
      return ortools::StatusBuilder(absl::StatusCode::kInvalidArgument)
             << " Callback " << data.event()
             << " is not allowed to generate cuts or lazy constraints";
    }

    // Since we cannot add ranged cuts, we must add ranged cuts as two
    // different cuts, one for each direction.
    char senseToAdd[2];
    double rhsToAdd[2];
    int numToAdd = 0;
    double const lb = cut.lower_bound();
    double const ub = cut.upper_bound();

    // Now process the cut/lazy constraint
    if (lb <= -1e20 && ub >= 1e20) {
      // Ignore free rows.
      continue;
    } else if (lb == ub) {
      // Equality constraint
      senseToAdd[0] = 'E';
      rhsToAdd[0] = lb;
      numToAdd = 1;
    } else {
      if (lb <= -1e20) {
        // <= constraint
        senseToAdd[0] = 'L';
        rhsToAdd[0] = ub;
        numToAdd = 1;
      } else if (ub >= 1e20) {
        // >= constraint
        senseToAdd[0] = 'G';
        rhsToAdd[0] = lb;
        numToAdd = 1;
      } else {
        // Range constraint, must insert the cut twice, once for each direction
        senseToAdd[0] = 'L';
        rhsToAdd[0] = ub;
        senseToAdd[1] = 'G';
        rhsToAdd[1] = lb;
        numToAdd = 2;
      }
    }

    for (int i = 0; i < numToAdd; ++i) {
      if (cut.is_lazy()) {
        // Lazy constraints are special. They must be provided in the
        // presolved space, so we must presolve them.
        *hadLazy = true;
        if (cols < 0) {
          // First lazy constraint. Initialize the buffers.
          OR_RETURN_IF_ERROR(Xpress::ToStatus(
              prob, XPRSgetintattrib(prob, XPRS_ORIGINALCOLS, &origCols)));
          OR_RETURN_IF_ERROR(
              Xpress::ToStatus(prob, XPRSgetintattrib(prob, XPRS_COLS, &cols)));
          origInd.reserve(origCols);
          origVal.reserve(origCols);
          preInd.resize(cols);
          preVal.resize(cols);
        }
        origInd.clear();
        origVal.clear();
        for (auto const [id, value] : MakeView(cut.linear_expression())) {
          origInd.push_back(or2xprs(ctx->varMap_, id));
          origVal.push_back(value);
        }
        int const cuttype = 0;
        int ncoefs, status;
        double preRhs;
        /* Note: For the second iteration for ranged rows, we cannot just
         *       reuse results from the first iteration and change direction
         *       of the constraints: constants on the left-hand side may have
         *       to be factored into the right-hand side.
         */
        OR_RETURN_IF_ERROR(Xpress::ToStatus(
            prob,
            XPRSpresolverow(prob, senseToAdd[i], origInd.size(), origInd.data(),
                            origVal.data(), rhsToAdd[i], cols, &ncoefs,
                            preInd.data(), preVal.data(), &preRhs, &status)));
        if (status != 0) {
          // Presolving a row may fail depending on which presolve reductions
          // have been carried out on the problem. Hedge against this.
          return ortools::StatusBuilder(absl::StatusCode::kInvalidArgument)
                 << "Failed to presolve a lazy constraint, status = " << status;
        }
        if (lazyStore) {
          // We cannot apply the lazy constraints directly, we must buffer
          // them.
          lazyStore->start.push_back(lazyStore->ind.size());
          lazyStore->ind.insert(lazyStore->ind.end(), preInd.begin(),
                                preInd.begin() + ncoefs);
          lazyStore->val.insert(lazyStore->val.end(), preVal.begin(),
                                preVal.begin() + ncoefs);
          lazyStore->sense.push_back(senseToAdd[i]);
          lazyStore->rhs.push_back(preRhs);
        } else {
          // Apply lazy constraints one by one, there is not really a point
          // in buffering since presolving the row already is a significant
          // overhead.
          XPRSint64 const prestart[] = {0,
                                        static_cast<XPRSint64>(preInd.size())};
          OR_RETURN_IF_ERROR(Xpress::ToStatus(
              prob, XPRSaddcuts64(prob, 1, &cuttype, &senseToAdd[i], &preRhs,
                                  prestart, preInd.data(), preVal.data())));
        }
      } else {
        // A regular cut.
        cutStore->start.push_back(cutStore->ind.size());
        for (auto const [id, value] : MakeView(cut.linear_expression())) {
          cutStore->ind.push_back(or2xprs(ctx->varMap_, id));
          cutStore->val.push_back(value);
        }
        cutStore->sense.push_back(senseToAdd[i]);
        cutStore->rhs.push_back(rhsToAdd[i]);
      }
    }
  }

  if (!cutsToCommit.start.empty())
    OR_RETURN_IF_ERROR(cutsToCommit.AddManagedCuts(prob));

  // Process any solutions that were added.
  for (SparseDoubleVectorProto const& solution_vector :
       result.suggested_solutions()) {
    std::vector<int> ids;
    std::vector<double> vals;
    for (auto const [id, value] : MakeView(solution_vector)) {
      ids.push_back(or2xprs(ctx->varMap_, id));
      vals.push_back(value);
    }
    if (solStore) {
      solStore->push_back({ids, vals});
    } else {
      OR_RETURN_IF_ERROR(Xpress::ToStatus(
          prob,
          XPRSaddmipsol(prob, ids.size(), vals.data(), ids.data(), nullptr)));
    }
  }
  // If we are asked to terminate then do that now.
  if (result.terminate()) {
    OR_RETURN_IF_ERROR(
        Xpress::ToStatus(prob, XPRSinterrupt(prob, XPRS_STOP_USER)));
  }
  return absl::OkStatus();
}

// Below we define various callbacks that we need in order to implement
// the ortools callback.
// ortools only has a single callback that is called with different events.
// Xpress on the other side has a separate callback for each event. So we
// need multiple Xpress callbacks that all fire the same ortools callback but
// with different events.

/** Barrier Logging callback.
 * This is used to implemented CALLBACK_EVENT_BARRIER.
 * Specification of this event from math_opt/cpp/callback.h:
 *     Called in each iterate of an interior point/barrier method.
 * @return non-zero to stop the solve.
 */
DEFINE_SCOPED_CB(Barlog, OrtoolsCallbackContext*, int,
                 (XPRSprob prob, void* cbdata)) {
  BarlogScopedCb* cb = reinterpret_cast<BarlogScopedCb*>(cbdata);
  OrtoolsCallbackContext* ctx = cb->or_tools_cb;
  CallbackDataProto cbargs;
  cbargs.set_event(CALLBACK_EVENT_BARRIER);
  OR_RETURN_IF_ERROR(InitBarrierStats(prob, cbargs));
  OR_RETURN_IF_ERROR(
      InvokeOrtoolsCallback(ctx, prob, cbargs, false, false, nullptr));
  return absl::OkStatus();
}

/** LP Logging callback.
 * This is used to implemented CALLBACK_EVENT_SIMPLEX.
 * Specification of this event from math_opt/cpp/callback.h:
 *     The solver is currently running the simplex method.
 * @return non-zero to stop the solve.
 */
DEFINE_SCOPED_CB(Lplog, OrtoolsCallbackContext*, int,
                 (XPRSprob prob, void* cbdata)) {
  LplogScopedCb* cb = reinterpret_cast<LplogScopedCb*>(cbdata);
  OrtoolsCallbackContext* ctx = cb->or_tools_cb;
  CallbackDataProto cbargs;
  cbargs.set_event(CALLBACK_EVENT_SIMPLEX);
  OR_RETURN_IF_ERROR(InitSimplexStats(prob, cbargs));
  OR_RETURN_IF_ERROR(
      InvokeOrtoolsCallback(ctx, prob, cbargs, false, false, nullptr));
  return absl::OkStatus();
}

/** Presolve callback.
 * This is used to implement CALLBACK_EVENT_PRESOLVE.
 * Specification of this event from math_opt/cpp/callback.h:
 *     The solver is currently running presolve
 * Note that this callback is fired only once at the end of presolve.
 * Note that in case of restarts it might be fired once for each restart.
 * @return non-zero to stop the solve.
 */
DEFINE_SCOPED_CB(Presolve, OrtoolsCallbackContext*, void,
                 (XPRSprob prob, void* cbdata)) {
  PresolveScopedCb* cb = reinterpret_cast<PresolveScopedCb*>(cbdata);
  OrtoolsCallbackContext* ctx = cb->or_tools_cb;
  CallbackDataProto cbargs;
  cbargs.set_event(CALLBACK_EVENT_PRESOLVE);
  bool hadLazy = false;
  OR_RETURN_IF_ERROR(InitPresolveStats(prob, cbargs));
  OR_RETURN_IF_ERROR(
      InvokeOrtoolsCallback(ctx, prob, cbargs, true, false, &hadLazy));
  return absl::OkStatus();
}

/** Preintsol callback.
 * This is used to implement CALLBACK_EVENT_MIPSOLUTION.
 * Specification of this event from math_opt/cpp/callback.h:
 *     Called every time a new MIP incumbent is found.
 * Note that we can only add solutions, cuts, lazy constraints if soltype==0.
 * In all other cases we capture what we should add and flush it only later
 * from optnode/cutround.
 */
DEFINE_SCOPED_CB(PreIntSol, OrtoolsCallbackContext*, void,
                 (XPRSprob prob, void* cbdata, int soltype, int* p_reject,
                  double*)) {
  // We could flush here, but that can have bad side effects. For example
  // - if we inject lazy constraints and the solution is not violated by
  //   any of them, then this would still reject the solution.
  // - if we flush a feasible solution but the current node is later
  //   rejected, then we lost this feasible solution.
  // We therefore leave all the flushing to the optnode callback.
  PreIntSolScopedCb* cb = reinterpret_cast<PreIntSolScopedCb*>(cbdata);
  OrtoolsCallbackContext* ctx = cb->or_tools_cb;
  CallbackDataProto cbargs;
  cbargs.set_event(CALLBACK_EVENT_MIP_SOLUTION);
  /* The specification in callback.h says that primal_solution should hold
   * the candidate relaxation. That is returned by XPRSgetcallbacksolution() in
   * the preintsol callback context.
   */
  int cols;
  OR_RETURN_IF_ERROR(
      Xpress::ToStatus(prob, XPRSgetintattrib(prob, XPRS_ORIGINALCOLS, &cols)));
  std::vector<double> x(cols);
  OR_RETURN_IF_ERROR(Xpress::ToStatus(
      prob, XPRSgetcallbacksolution(prob, nullptr, x.data(), 0, cols - 1)));
  // Note that ortools has no way to communicate the objective value,
  // users have to find that themselves.
  *cbargs.mutable_primal_solution_vector() =
      ctx->FilterSolution(absl::MakeSpan(x), ctx->mip_solution_filter_);
  bool hadLazy = false;
  OR_RETURN_IF_ERROR(InitMipStats(prob, cbargs));
  int solution_source = CALLBACK_SOLUTION_SOURCE_UNSPECIFIED;
  switch (soltype) {
    case 0:
      solution_source = CALLBACK_SOLUTION_SOURCE_INTEGRAL;
      break;
    case 1:
      solution_source = CALLBACK_SOLUTION_SOURCE_HEURISTIC;
      break;
    case 2:
      solution_source = CALLBACK_SOLUTION_SOURCE_USER;
      break;
  }
  cbargs.mutable_mip_stats()->set_solution_source(solution_source);
  OR_RETURN_IF_ERROR(
      InvokeOrtoolsCallback(ctx, prob, cbargs, true, soltype == 0, &hadLazy));
  if (hadLazy && soltype != 0) {
    // The user provided lazy constraints but we could not add them. We have to
    // flatly reject the solution.
    // Note that we must NOT set *p_reject=1 if there were lazy constraints
    // and we could add them, since that would drop the whole subtree.
    *p_reject = 1;
  }
  return absl::OkStatus();
}

/** Cut round callback.
 * While the optnode() callback is the obvious candidate for implementing
 * CALLBACK_EVENT_MIP_NODE, we use the cutround callback because this allows
 * us to immediately directly
 * - add user cuts via XPRSaddmanagedcuts(),
 * - add lazy constraints via XPRSaddcuts() (it is recommended to do this via
 *   optnode() but explicitly allowed for cutround())
 * - add new solutions via XPRSaddmipsol().
 * In the optnode() callback we cannot call XPRSaddmanagedcuts().
 */
DEFINE_SCOPED_CB(CutRound, OrtoolsCallbackContext*, void,
                 (XPRSprob prob, void* cbdata, int ifxpresscuts, int*)) {
  CutRoundScopedCb* cb = reinterpret_cast<CutRoundScopedCb*>(cbdata);
  OrtoolsCallbackContext* ctx = cb->or_tools_cb;

  // First flush any pending information.
  OR_RETURN_IF_ERROR(ctx->FlushDelayedInfo(prob, true));

  // Only trigger the event if Xpress is done with its cutting.
  if (ifxpresscuts) return absl::OkStatus();

  CallbackDataProto cbargs;
  cbargs.set_event(CALLBACK_EVENT_MIP_NODE);
  // The specification in callback.h says that primal_solution should hold
  // the LP relaxation. That is returned by XPRSgetcallbacksolution() in the
  // optnode callback context. If the current node is infeasible then we shall
  // not setup primal_solution.
  int cols;
  OR_RETURN_IF_ERROR(
      Xpress::ToStatus(prob, XPRSgetintattrib(prob, XPRS_ORIGINALCOLS, &cols)));
  std::vector<double> x(cols);
  int available = 0;
  OR_RETURN_IF_ERROR(Xpress::ToStatus(
      prob, XPRSgetcallbacksolution(prob, &available, x.data(), 0, cols - 1)));
  if (available) {
    *cbargs.mutable_primal_solution_vector() =
        ctx->FilterSolution(absl::MakeSpan(x), ctx->mip_node_filter_);
  }
  bool hadLazy = false;
  OR_RETURN_IF_ERROR(InitMipStats(prob, cbargs));
  OR_RETURN_IF_ERROR(
      InvokeOrtoolsCallback(ctx, prob, cbargs, true, true, &hadLazy));
  return absl::OkStatus();
}

/** prenode callback.
 * This is used to implement CALLBACK_EVENT_MIP.
 * Specification of this event from math_opt/cpp/callback.h:
 *     The solver is in the MIP loop (called periodically before starting a
 *     new node). Useful for early termination. Note that this event does not
 *     provide information on LP relaxations nor about new incumbent solutions.
 */
DEFINE_SCOPED_CB(PreNode, OrtoolsCallbackContext*, void,
                 (XPRSprob prob, void* cbdata, int* p_infeasible)) {
  PreNodeScopedCb* cb = reinterpret_cast<PreNodeScopedCb*>(cbdata);
  OrtoolsCallbackContext* ctx = cb->or_tools_cb;
  CallbackDataProto cbargs;
  cbargs.set_event(CALLBACK_EVENT_MIP);
  bool hadLazy = false;
  /** TODO: Is it not clear from the specification whether injecting cuts,
   *        lazy constraints, or feasible solutions is allowed from this
   *        event. We currently allow that but defer the actual injection
   *        until the next optnode() event.
   */
  OR_RETURN_IF_ERROR(InitMipStats(prob, cbargs));
  OR_RETURN_IF_ERROR(
      InvokeOrtoolsCallback(ctx, prob, cbargs, true, false, &hadLazy));
  return absl::OkStatus();
}

/** optnode callback.
 * This is only used for flushing information that could not be processed in
 * preintsol() or prenode().
 */
DEFINE_SCOPED_CB(OptNode, OrtoolsCallbackContext*, void,
                 (XPRSprob prob, void* cbdata, int* p_infeasible)) {
  OptNodeScopedCb* cb = reinterpret_cast<OptNodeScopedCb*>(cbdata);
  OrtoolsCallbackContext* ctx = cb->or_tools_cb;

  OR_RETURN_IF_ERROR(ctx->FlushDelayedInfo(prob, false));
  return absl::OkStatus();
}

/** An ortools message callback that prints everything to stdout. */
static void stdoutMessageCallback(std::vector<std::string> const& lines) {
  for (auto& l : lines) std::cout << l << '\n';
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
  /** Installed barlog callback (if any). */
  BarlogScopedCb barlogCallback;
  /** Installed lplog callback (if any). */
  LplogScopedCb lplogCallback;
  /** Installed presolve callback (if any). */
  PresolveScopedCb presolveCallback;
  /** Installed preintsol callback (if any). */
  PreIntSolScopedCb preintsolCallback;
  /** Installed optnode callback (if any). */
  OptNodeScopedCb optnodeCallback;
  /** Installed prenode callback (if any). */
  PreNodeScopedCb prenodeCallback;
  /** Installed cutround callback (if any). */
  CutRoundScopedCb cutroundCallback;
  /** If we installed an interrupter callback then this removes it. */
  std::function<void()> removeInterrupterCallback;
  /** Context to invoke the ortools callback (if any). */
  std::unique_ptr<OrtoolsCallbackContext> ctx;
  /** A single control that must be reset in the destructor. */
  struct OneControl {
    int id;
    // std::variant (not a raw union) so the std::string alternative is
    // destroyed automatically; index() drives the manual restore in the dtor.
    std::variant<int64_t, double, std::string> value;
    enum {
      INT_CONTROL,
      DBL_CONTROL,
      STR_CONTROL
    };  // Matches std::variant<>::index;
  };
  /** Controls to be reset in the destructor. */
  std::vector<OneControl> modifiedControls;

 public:
  ScopedSolverContext(Xpress* xpress)
      : shared_ctx(xpress), removeInterrupterCallback(nullptr), ctx(nullptr) {}
  absl::Status Set(int id, int32_t value) { return Set(id, int64_t(value)); }
  absl::Status Set(int id, int64_t value) {
    OR_ASSIGN_OR_RETURN(int64_t old, shared_ctx.xpress->GetIntControl64(id));
    modifiedControls.push_back({id, old});
    OR_RETURN_IF_ERROR(shared_ctx.xpress->SetIntControl64(id, value));
    return absl::OkStatus();
  }
  absl::Status Set(int id, double value) {
    OR_ASSIGN_OR_RETURN(double old, shared_ctx.xpress->GetDblControl(id));
    modifiedControls.push_back({id, old});
    OR_RETURN_IF_ERROR(shared_ctx.xpress->SetDblControl(id, value));
    return absl::OkStatus();
  }
  absl::Status Set(int id, std::string const& value) {
    OR_ASSIGN_OR_RETURN(std::string old, shared_ctx.xpress->GetStrControl(id));
    modifiedControls.push_back({id, old});
    OR_RETURN_IF_ERROR(shared_ctx.xpress->SetStrControl(id, value));
    return absl::OkStatus();
  }

  absl::Status AddCallbacks(
      MessageCallback message_callback,
      const CallbackRegistrationProto& callback_registration,
      SolverInterface::Callback callback, const SolveInterrupter* interrupter,
      absl::linked_hash_map<XpressSolver::VarId,
                            XpressSolver::XpressVariableIndex> const& varMap) {
    if (message_callback)
      OR_RETURN_IF_ERROR(messageCallback.Add(&shared_ctx, message_callback));
    if (interrupter) {
      /* To be extra safe we add two ways to interrupt Xpress:
       * 1. We register a checktime callback that polls the interrupter.
       * 2. We register a callback with the interrupter that will call
       *    XPRSinterrupt().
       * Eventually we should assess whether the first thing is a performance
       * hit and if so, remove it.
       */
      OR_RETURN_IF_ERROR(checktimeCallback.Add(&shared_ctx, interrupter));
      SolveInterrupter::CallbackId const id =
          interrupter->AddInterruptionCallback([this] {
            CHECK_OK(shared_ctx.xpress->Interrupt(XPRS_STOP_USER));
          });
      removeInterrupterCallback = [=] {
        interrupter->RemoveInterruptionCallback(id);
      };
    }
    if (callback) {
      absl::flat_hash_set<CallbackEventProto> const events =
          EventSet(callback_registration);

      // If the user wants to add cuts or listen to CALLBACK_EVENT_MIP_NODE
      // then we need XPRSaddcbcutround().
      if ((events.contains(CALLBACK_EVENT_MIP_NODE) ||
           callback_registration.add_cuts()) &&
          !XPRSaddcbcutround) {
        return ortools::StatusBuilder(absl::StatusCode::kInvalidArgument)
               << "Callback will add cuts or listen for "
                  "CALLBACK_EVENT_MIP_NODE but XPRSaddcbcutround() is not "
                  "available. Need at least Xpress optimizer version 45 "
                  "(Xpress 9.6.0)";
      }

      // If the user wants to add cuts but XPRSaddmanagedcuts() is not available
      // then we error out.
      if (callback_registration.add_cuts() && !XPRSaddmanagedcuts64) {
        return ortools::StatusBuilder(absl::StatusCode::kInvalidArgument)
               << "Callback will add cuts but XPRSaddmanagedcuts64() is not "
                  "available. Need at least Xpress optimizer version 45 "
                  "(Xpress 9.6.0)";
      }

      ctx = std::make_unique<OrtoolsCallbackContext>(
          callback, callback_registration, varMap);

      // Register the callbacks that we need to handle the required events.
      // If we listen to any MIP event but CALLBACK_EVENT_MIP_NODE then we
      // may have to flush info at the end of a node.
      bool needFlush = false;
      for (auto const& event : events) {
        switch (event) {
          case CALLBACK_EVENT_PRESOLVE:
            OR_RETURN_IF_ERROR(presolveCallback.Add(&shared_ctx, ctx.get()));
            break;
          case CALLBACK_EVENT_SIMPLEX:
            OR_RETURN_IF_ERROR(lplogCallback.Add(&shared_ctx, ctx.get()));
            break;
          case CALLBACK_EVENT_MIP:
            OR_RETURN_IF_ERROR(prenodeCallback.Add(&shared_ctx, ctx.get()));
            needFlush = true;
            break;
          case CALLBACK_EVENT_MIP_SOLUTION:
            OR_RETURN_IF_ERROR(preintsolCallback.Add(&shared_ctx, ctx.get()));
            needFlush = true;
            break;
          case CALLBACK_EVENT_MIP_NODE:
            OR_RETURN_IF_ERROR(cutroundCallback.Add(&shared_ctx, ctx.get()));
            break;
          case CALLBACK_EVENT_BARRIER:
            OR_RETURN_IF_ERROR(barlogCallback.Add(&shared_ctx, ctx.get()));
            break;
          case CALLBACK_EVENT_UNSPECIFIED:  // fallthrough
          default:
            LOG(FATAL) << "Unsupported callback event: " << event;
        }
      }

      if (needFlush) {
        OR_RETURN_IF_ERROR(optnodeCallback.Add(&shared_ctx, ctx.get()));
      }

      // If the user plans to ever add lazy constraints from callbacks then we
      // must disable dual reductions.
      if (callback_registration.add_lazy_constraints()) {
        OR_RETURN_IF_ERROR(
            shared_ctx.xpress->SetIntControl(XPRS_MIPDUALREDUCTIONS, 0));
      }

      /** TODO: Do we have a control that makes it more likely that crushing a
       *        form will never fail? Gurobi and CPLEX have this.
       */
    }
    return absl::OkStatus();
  }
  /** Setup model specific parameters. */
  absl::Status ApplyParameters(const SolveParametersProto& parameters,
                               MessageCallback message_callback,
                               std::string* export_model, bool* force_postsolve,
                               bool* stop_after_lp) {
    std::vector<std::string> warnings;
    OR_ASSIGN_OR_RETURN(bool const isMIP, shared_ctx.xpress->IsMIP());
    if (parameters.enable_output()) {
      // This is considered only if no message callback is set, see the
      // ortools specification of the enable_output parameter.
      if (!message_callback) {
        OR_RETURN_IF_ERROR(
            messageCallback.Add(&shared_ctx, stdoutMessageCallback));
      }
    }
    absl::Duration time_limit = absl::InfiniteDuration();
    if (parameters.has_time_limit()) {
      OR_ASSIGN_OR_RETURN(
          time_limit, util_time::DecodeGoogleApiProto(parameters.time_limit()));
    }
    if (time_limit < absl::InfiniteDuration()) {
      OR_RETURN_IF_ERROR(
          Set(XPRS_TIMELIMIT, absl::ToDoubleSeconds(time_limit)));
    }
    if (parameters.has_iteration_limit()) {
      if (parameters.lp_algorithm() == LP_ALGORITHM_FIRST_ORDER) {
        // Iteration limit for PDHG is BARHGMAXRESTARTS
        OR_RETURN_IF_ERROR(
            Set(XPRS_BARHGMAXRESTARTS, parameters.iteration_limit()));
      } else {
        OR_RETURN_IF_ERROR(Set(XPRS_LPITERLIMIT, parameters.iteration_limit()));
        OR_RETURN_IF_ERROR(
            Set(XPRS_BARITERLIMIT, parameters.iteration_limit()));
      }
    }
    if (parameters.has_node_limit()) {
      OR_RETURN_IF_ERROR(Set(XPRS_MAXNODE, parameters.node_limit()));
    }
    if (parameters.has_cutoff_limit()) {
      OR_RETURN_IF_ERROR(Set(XPRS_MIPABSCUTOFF, parameters.cutoff_limit()));
    }
    if (parameters.has_objective_limit()) {
      // In Xpress you can apply MIPABSCUTOFF also to LPs.
      // However, ortools applies both cutoff_limit and objective_limit
      // to LPs and distinguishes the two, i.e., expect different return
      // values depending on what is set. Since we cannot easily make this
      // distinction, we do not support objective_limit. Users should just
      // use cutoff_limit with LPs as well.
      warnings.emplace_back(
          "XpressSolver does not support objective_limit; use cutoff_limit "
          "instead");
    }
    if (parameters.has_best_bound_limit()) {
      warnings.emplace_back("XpressSolver does not support best_bound_limit");
    }
    if (parameters.has_solution_limit()) {
      OR_RETURN_IF_ERROR(Set(XPRS_MAXMIPSOL, parameters.solution_limit()));
    }
    if (parameters.has_threads() && parameters.threads() > 0)
      OR_RETURN_IF_ERROR(Set(XPRS_THREADS, parameters.threads()));
    if (parameters.has_random_seed()) {
      OR_RETURN_IF_ERROR(Set(XPRS_RANDOMSEED, parameters.random_seed()));
    }
    if (parameters.has_absolute_gap_tolerance())
      OR_RETURN_IF_ERROR(
          Set(XPRS_MIPABSSTOP, parameters.absolute_gap_tolerance()));
    if (parameters.has_relative_gap_tolerance())
      OR_RETURN_IF_ERROR(
          Set(XPRS_MIPRELSTOP, parameters.relative_gap_tolerance()));
    if (parameters.has_solution_pool_size()) {
      warnings.emplace_back("XpressSolver does not support solution_pool_size");
    }
    // According to the documentation, LP algorithm is only for LPs
    if (!isMIP && parameters.lp_algorithm() != LP_ALGORITHM_UNSPECIFIED) {
      switch (parameters.lp_algorithm()) {
        case LP_ALGORITHM_PRIMAL_SIMPLEX:
          OR_RETURN_IF_ERROR(Set(XPRS_LPFLAGS, 1 << 1));
          break;
        case LP_ALGORITHM_DUAL_SIMPLEX:
          OR_RETURN_IF_ERROR(Set(XPRS_LPFLAGS, 1 << 0));
          break;
        case LP_ALGORITHM_BARRIER:
          OR_RETURN_IF_ERROR(Set(XPRS_LPFLAGS, 1 << 2));
          break;
        case LP_ALGORITHM_FIRST_ORDER:
          OR_RETURN_IF_ERROR(Set(XPRS_LPFLAGS, 1 << 2));
          OR_RETURN_IF_ERROR(Set(XPRS_BARALG, 4));
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
          OR_RETURN_IF_ERROR(Set(XPRS_PRESOLVE, 0));  // Turn presolve off
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
        OR_RETURN_IF_ERROR(Set(XPRS_PRESOLVEPASSES, presolvePasses));
    }
    if (parameters.cuts() != EMPHASIS_UNSPECIFIED) {
      switch (parameters.cuts()) {
        case EMPHASIS_OFF:
          OR_RETURN_IF_ERROR(Set(XPRS_CUTSTRATEGY, 0));
          break;
        case EMPHASIS_LOW:
          OR_RETURN_IF_ERROR(Set(XPRS_CUTSTRATEGY, 1));
          break;
        case EMPHASIS_MEDIUM:
          OR_RETURN_IF_ERROR(Set(XPRS_CUTSTRATEGY, 2));
          break;
        case EMPHASIS_HIGH:
          OR_RETURN_IF_ERROR(Set(XPRS_CUTSTRATEGY, 3));
          break;
        case EMPHASIS_VERY_HIGH:
          OR_RETURN_IF_ERROR(Set(XPRS_CUTSTRATEGY, 3));  // Same as high
          break;
      }
    }
    if (parameters.heuristics() != EMPHASIS_UNSPECIFIED) {
      switch (parameters.heuristics()) {
        case EMPHASIS_OFF:
          OR_RETURN_IF_ERROR(Set(XPRS_HEUREMPHASIS, 0));
          break;
        case EMPHASIS_UNSPECIFIED:
          break;
        case EMPHASIS_LOW:  // fallthrough
        case EMPHASIS_MEDIUM:
          OR_RETURN_IF_ERROR(Set(XPRS_HEUREMPHASIS, 1));
          break;
        case EMPHASIS_HIGH:  // fallthrough
        case EMPHASIS_VERY_HIGH:
          OR_RETURN_IF_ERROR(Set(XPRS_HEUREMPHASIS, 2));
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

      if (name == "EXPORT_MODEL") {
        if (export_model) *export_model = value;
        continue;
      } else if (name == "FORCE_POSTSOLVE") {
        if (!absl::SimpleAtoi(value, &l))
          return ortools::InvalidArgumentErrorBuilder()
                 << "value " << value << " for FORCE_POSTSOLVE"
                 << " is not an integer";
        if (force_postsolve) *force_postsolve = l != 0;
        continue;
      } else if (name == "STOP_AFTER_LP") {
        if (!absl::SimpleAtoi(value, &l))
          return ortools::InvalidArgumentErrorBuilder()
                 << "value " << value << " for STOP_AFTER_LP"
                 << " is not an integer";
        if (stop_after_lp) *stop_after_lp = l != 0;
        continue;
      }
      OR_RETURN_IF_ERROR(
          shared_ctx.xpress->GetControlInfo(name.c_str(), &id, &type));
      switch (type) {
        case XPRS_TYPE_INT:  // fallthrough
        case XPRS_TYPE_INT64:
          if (!absl::SimpleAtoi(value, &l))
            return ortools::InvalidArgumentErrorBuilder()
                   << "value " << value << " for " << name
                   << " is not an integer";
          if (type == XPRS_TYPE_INT && (l > std::numeric_limits<int>::max() ||
                                        l < std::numeric_limits<int>::min()))
            return ortools::InvalidArgumentErrorBuilder()
                   << "value " << value << " for " << name
                   << " is out of range";
          OR_RETURN_IF_ERROR(Set(id, l));
          break;
        case XPRS_TYPE_DOUBLE:
          if (!absl::SimpleAtod(value, &d))
            return ortools::InvalidArgumentErrorBuilder()
                   << "value " << value << " for " << name
                   << " is not a floating pointer number";
          OR_RETURN_IF_ERROR(Set(id, d));
          break;
        case XPRS_TYPE_STRING:
          OR_RETURN_IF_ERROR(Set(id, value));
          break;
        default:
          return ortools::InvalidArgumentErrorBuilder()
                 << "bad control type for " << name;
      }
    }

    if (!warnings.empty()) {
      return absl::InvalidArgumentError(absl::StrJoin(warnings, "; "));
    }
    return absl::OkStatus();
  }
  absl::Status ApplyModelParameters(
      ModelSolveParametersProto const& model_parameters,
      absl::linked_hash_map<XpressSolver::VarId,
                            XpressSolver::XpressVariableIndex> const&
          variables_map,
      absl::linked_hash_map<XpressSolver::LinearConstraintId,
                            XpressSolver::LinearConstraintData> const&
          linear_constraints_map,
      absl::linked_hash_map<XpressSolver::AuxiliaryObjectiveId,
                            XpressSolver::XpressMultiObjectiveIndex> const&
          objectives_map) {
    OR_ASSIGN_OR_RETURN(int const cols,
                        shared_ctx.xpress->GetIntAttr(XPRS_ORIGINALCOLS));
    OR_ASSIGN_OR_RETURN(int const rows,
                        shared_ctx.xpress->GetIntAttr(XPRS_ORIGINALROWS));
    // Set initial basis
    if (model_parameters.has_initial_basis()) {
      // XPRSloadbasis() will raise an error if called on a model in presolved
      // state. We still trap this already here so that we can produce a more
      // meaningful error message.
      OR_ASSIGN_OR_RETURN(int const state,
                          shared_ctx.xpress->GetIntAttr(XPRS_PRESOLVESTATE));
      if (state & ((1 << 1) | (1 << 2))) {
        return ortools::InvalidArgumentErrorBuilder()
               << "cannot set basis for model in presolved space (consider "
                  "FORCE_POSTSOLVE?)";
      }
      auto const& basis = model_parameters.initial_basis();
      std::vector<int> xpress_var_basis_status(cols);
      for (const auto [id, value] : MakeView(basis.variable_status())) {
        xpress_var_basis_status[or2xprs(variables_map, id)] =
            MathOptToXpressBasisStatus(static_cast<BasisStatusProto>(value),
                                       false);
      }
      std::vector<int> xpress_constr_basis_status(rows);
      for (const auto [id, value] : MakeView(basis.constraint_status())) {
        xpress_constr_basis_status[or2xprs(linear_constraints_map, id)] =
            MathOptToXpressBasisStatus(static_cast<BasisStatusProto>(value),
                                       true);
      }
      OR_RETURN_IF_ERROR(shared_ctx.xpress->SetStartingBasis(
          xpress_constr_basis_status, xpress_var_basis_status));
    }
    std::vector<int> colind;

    // Install solution hints. Xpress does not explicitly have solution
    // hints but it supports partial MIP starts. So we just add each solution
    // hint as MIP start.
    if (model_parameters.solution_hints_size() > 0) {
      unsigned int cnt = 0;
      std::vector<double> mipStart;
      colind.reserve(cols);
      mipStart.reserve(cols);
      for (auto const& hint : model_parameters.solution_hints()) {
        colind.clear();
        mipStart.clear();
        for (const auto [id, value] : MakeView(hint.variable_values())) {
          colind.push_back(or2xprs(variables_map, id));
          mipStart.push_back(value);
        }
        if (mipStart.size() > cols)
          return ortools::InvalidArgumentErrorBuilder()
                 << "more solution hints than columns";
        // XPRSaddmipsol() expects a solution in the original space
        OR_RETURN_IF_ERROR(shared_ctx.xpress->AddMIPSol(
            mipStart, colind, absl::StrCat("SolutionHint", cnt).c_str()));
        ++cnt;
      }
    }

    // Install branching priorities.
    if (model_parameters.has_branching_priorities()) {
      auto const& prios = model_parameters.branching_priorities();
      colind.clear();
      colind.reserve(prios.ids_size());
      std::vector<int> priority;
      priority.reserve(prios.ids_size());
      for (const auto [id, prio] : MakeView(prios)) {
        colind.push_back(or2xprs(variables_map, id));
        // Xpress only allows priorities in [0,1000].
        // In ortools higher priority takes precedence while in Xpress
        // lower priority takes precedence.
        if (prio < 0 || prio > 1000)
          return ortools::InvalidArgumentErrorBuilder()
                 << "Xpress only allows branching priorities in [0,1000]";
        priority.push_back(
            1000 - prio);  // Smaller prios have higher precedence in Xpress!
      }

      OR_RETURN_IF_ERROR(shared_ctx.xpress->LoadDirs(
          absl::MakeSpan(colind), absl::MakeSpan(priority), std::nullopt,
          std::nullopt, std::nullopt));
    }

    // Objective parameters: primary/single objective
    if (model_parameters.has_primary_objective_parameters()) {
      auto const& p = model_parameters.primary_objective_parameters();
      // Objective violation tolerances only need to be installed for
      // multi-objective models. We just set them blindly here. They don't
      // hurt for a single-objective model.
      if (p.has_objective_degradation_absolute_tolerance()) {
        OR_RETURN_IF_ERROR(shared_ctx.xpress->SetObjectiveDoubleControl(
            0, XPRS_OBJECTIVE_ABSTOL,
            p.objective_degradation_absolute_tolerance()));
      }
      if (p.has_objective_degradation_relative_tolerance()) {
        OR_RETURN_IF_ERROR(shared_ctx.xpress->SetObjectiveDoubleControl(
            0, XPRS_OBJECTIVE_RELTOL,
            p.objective_degradation_relative_tolerance()));
      }
      if (p.has_time_limit()) {
        // We support a time limit but only if there is one single objective.
        if (objectives_map.size() > 0) {
          return ortools::InvalidArgumentErrorBuilder()
                 << "Xpress does not support per-objective time limits";
        }
        OR_ASSIGN_OR_RETURN(auto l,
                            util_time::DecodeGoogleApiProto(p.time_limit()));

        OR_RETURN_IF_ERROR(shared_ctx.xpress->SetDblControl(
            XPRS_TIMELIMIT, absl::ToDoubleSeconds(l)));
      }
    }
    // Objective parameters: auxiliary objectives
    for (auto const& [id, p] :
         model_parameters.auxiliary_objective_parameters()) {
      if (p.has_objective_degradation_absolute_tolerance()) {
        OR_RETURN_IF_ERROR(shared_ctx.xpress->SetObjectiveDoubleControl(
            or2xprs(objectives_map, id), XPRS_OBJECTIVE_ABSTOL,
            p.objective_degradation_absolute_tolerance()));
      }
      if (p.has_objective_degradation_relative_tolerance()) {
        OR_RETURN_IF_ERROR(shared_ctx.xpress->SetObjectiveDoubleControl(
            or2xprs(objectives_map, id), XPRS_OBJECTIVE_RELTOL,
            p.objective_degradation_relative_tolerance()));
      }
      if (p.has_time_limit()) {
        return ortools::InvalidArgumentErrorBuilder()
               << "Xpress does not support per-objective time limits";
      }
    }

    if (model_parameters.lazy_linear_constraint_ids_size() > 0) {
      std::vector<int> delayedRows;
      delayedRows.reserve(rows);
      for (auto const& idx : model_parameters.lazy_linear_constraint_ids()) {
        delayedRows.push_back(or2xprs(linear_constraints_map, idx));
      }
      if (delayedRows.size() > rows)
        return ortools::InvalidArgumentErrorBuilder()
               << "more lazy constraints than rows";

      OR_RETURN_IF_ERROR(shared_ctx.xpress->LoadDelayedRows(delayedRows));
    }

    return absl::OkStatus();
  }
  /** Interrupt the current solve with the given reason. */
  void Interrupt(int reason) { CHECK_OK(shared_ctx.xpress->Interrupt(reason)); }

  absl::Status HandleCallbackProblems() {
    return shared_ctx.HandleCallbackProblems();
  }

  ~ScopedSolverContext() {
    for (auto it = modifiedControls.rbegin(); it != modifiedControls.rend();
         ++it) {
      switch (it->value.index()) {
        case OneControl::INT_CONTROL:
          CHECK_OK(shared_ctx.xpress->SetIntControl64(
              it->id, std::get<int64_t>(it->value)));
          break;
        case OneControl::DBL_CONTROL:
          CHECK_OK(shared_ctx.xpress->SetDblControl(
              it->id, std::get<double>(it->value)));
          break;
        case OneControl::STR_CONTROL:
          CHECK_OK(shared_ctx.xpress->SetStrControl(
              it->id, std::get<std::string>(it->value).c_str()));
          break;
      }
    }
    if (removeInterrupterCallback) removeInterrupterCallback();
  }
};

/** Different modes for ExtractSingleton(). */
enum class SingletonType {
  SOS,      /**< SOS constraint. */
  SOCBound, /**< Second order cone constraint bound. */
  SOCNorm   /**< Second order cone constraint norm. */
};

// ortools supports SOS constraints and second order cone constraints on
// expressions. Xpress only supports these constructs on singleton variables.
// We could create auxiliary variables here, set each of them equal to one of
// the expressions and then formulate SOS/SOC on the auxiliary variables.
// This however seems a bit of overkill at the moment, so we just error out
// if elements are non-singleton.
// Returns the variable of the singleton as return value and its coefficient
// in *p_coef.
absl::StatusOr<std::optional<XpressSolver::VarId>> ExtractSingleton(
    LinearExpressionProto const& expr, SingletonType type, double* p_coef) {
  double const constant = expr.offset();
  if (expr.ids_size() == 1 && constant == 0.0) {
    // We have a single variable in the expression and no constant.
    double const coef = expr.coefficients(0);
    switch (type) {
      case SingletonType::SOS:
        // A non-zero coefficient does not change anything, so is allowed.
        if (coef == 0.0) {
          return ortools::InvalidArgumentErrorBuilder()
                 << "Xpress does not support coefficient " << coef
                 << " in SOS (consider using auxiliary variables?)";
        }
        break;
      case SingletonType::SOCBound:  // fallthrough
      case SingletonType::SOCNorm:
        // We are going to square the coefficient, so anything non-negative
        // is allowed.
        if (coef < 0) {
          return ortools::InvalidArgumentErrorBuilder()
                 << "Xpress does not support coefficient " << coef
                 << " in a second order cone constraint "
                 << (type == SingletonType::SOCBound ? "bound" : "norm")
                 << " (consider using auxiliary variables?)";
        }
        break;
    }
    if (p_coef) *p_coef = coef;
    return std::optional<XpressSolver::VarId>(expr.ids(0));
  } else if (expr.ids_size() == 0) {
    // The expression is constant.
    switch (type) {
      case SingletonType::SOS:
        // Any non-zero constant would force all other variables to 0.
        // Any zero constant would be redundant.
        // Both are edge cases that we do not support at the moment.
        return ortools::InvalidArgumentErrorBuilder()
               << "Xpress does not support constant expressions in SOS "
                  "(consider using auxiliary variables?)";
      case SingletonType::SOCBound:
        // We are going to square the bound, so it should not be negative.
        if (constant < 0.0) {
          return ortools::InvalidArgumentErrorBuilder()
                 << "Xpress does not support constant " << constant
                 << " in a second order cone constraint bound (consider using "
                    "auxiliary variables?)";
        }
        break;
      case SingletonType::SOCNorm:
        // Constant entries in the norm are not supported (we would have to
        // move them to the right-hand side).
        return ortools::InvalidArgumentErrorBuilder()
               << "Xpress does not support constants in a second order cone "
                  "constraint norm (consider using auxiliary variables?)";
    }
    if (p_coef) *p_coef = constant;
    return std::nullopt;
  } else {
    // Multiple coefficients
    static char const* const name[] = {"SOS",
                                       "second order cone constraint bound",
                                       "second order cone constraint norm"};
    return ortools::InvalidArgumentErrorBuilder()
           << "Xpress does not support general linear expressions in "
           << name[static_cast<int>(type)]
           << " (consider using auxiliary variables?)";
  }
}

/** Trait for AddNames() so that we can write it in a generic way.
 * The default implementation works for columns and rows.
 */
template <typename T>
struct NameResolver {
  static std::string const& GetName(T const& container, int i) {
    return container.names(i);
  }
};

/** Specialization for NameResolver for SOS. */
template <typename K, typename V>
struct NameResolver<google::protobuf::Map<K, V>> {
  static std::string const& GetName(
      google::protobuf::Map<K, V> const& container,
      typename google::protobuf::Map<K, V>::const_iterator const& i) {
    return i->second.name();
  }
};

/** Add names to an Xpress object.
 * Extracts the first count names from container.
 * It is assumed that the names are for elements offset, offset+1, offset+2, ...
 */
template <typename T, typename I>
absl::Status AddNames(Xpress* xpress, int type, int offset, I begin, I end,
                      T const& container) {
  std::vector<char> buffer;
  int i = 0, start = 0;
  while (begin != end) {
    std::string const& name = NameResolver<T>::GetName(container, begin);
    char const* c_name = name.c_str();
    buffer.insert(buffer.end(), c_name, c_name + name.size() + 1);
    // Add names in chunks of 1MB.
    if (buffer.size() > 1024 * 1024) {
      OR_RETURN_IF_ERROR(
          xpress->AddNames(type, buffer, offset + start, offset + i));
      start = i + 1;
      buffer.clear();
    }
    ++i;
    ++begin;
  }
  if (buffer.size()) {
    OR_RETURN_IF_ERROR(
        xpress->AddNames(type, buffer, offset + start, offset + i - 1));
  }
  return absl::OkStatus();
}

}  // namespace

constexpr SupportedProblemStructures kXpressSupportedStructures = {
    .integer_variables = SupportType::kSupported,
    .multi_objectives = SupportType::kSupported,
    .quadratic_objectives = SupportType::kSupported,
    .quadratic_constraints = SupportType::kSupported,
    // Limitation: We only implemented support for constraints of type
    //   norm(a1*x1,...,an*xn) <= a0*x0
    // General linear expressions in the norm or in the bound are not
    // supported at the moment. They must be emulated by the caller using
    // auxiliary variables. The right-hand side may be a constant.
    .second_order_cone_constraints = SupportType::kSupported,
    // Limitation: We only implemented support for SOS constraints on singleton
    // variables. General expressions in the SOS are not supported. They must
    // be emulated by the caller using auxiliary variables.
    .sos1_constraints = SupportType::kSupported,
    .sos2_constraints = SupportType::kSupported,
    .indicator_constraints = SupportType::kSupported};

absl::StatusOr<std::unique_ptr<XpressSolver>> XpressSolver::New(
    const ModelProto& model, const InitArgs& init_args) {
  if (!XpressIsCorrectlyInstalled()) {
    return absl::InvalidArgumentError("Xpress is not correctly installed.");
  }
  OR_RETURN_IF_ERROR(
      ModelIsSupported(model, kXpressSupportedStructures, "XPRESS"));

  // We can add here extra checks that are not made in ModelIsSupported
  // (for example, if XPRESS does not support multi-objective with quad terms)

  OR_ASSIGN_OR_RETURN(auto xpr, Xpress::New(model.name()));
  bool extract_names = init_args.streamable.has_xpress() &&
                       init_args.streamable.xpress().has_extract_names() &&
                       init_args.streamable.xpress().extract_names();
  auto xpress_solver =
      absl::WrapUnique(new XpressSolver(std::move(xpr), extract_names));
  OR_RETURN_IF_ERROR(xpress_solver->LoadModel(model));
  return xpress_solver;
}

absl::Status XpressSolver::LoadModel(const ModelProto& input_model) {
  CHECK(xpress_ != nullptr);
  OR_RETURN_IF_ERROR(xpress_->SetProbName(input_model.name()));
  OR_RETURN_IF_ERROR(AddNewVariables(input_model.variables()));
  OR_RETURN_IF_ERROR(AddNewLinearConstraints(input_model.linear_constraints()));
  OR_RETURN_IF_ERROR(
      ChangeCoefficients(input_model.linear_constraint_matrix()));
  OR_RETURN_IF_ERROR(AddObjective(input_model.objective(), std::nullopt,
                                  !input_model.auxiliary_objectives().empty()));
  // Tests expect an error on duplicate priorities, so raise one.
  // Xpress would otherwise merge objectives with the same objective when it
  // starts solving.
  absl::flat_hash_set<AuxiliaryObjectiveId> prios = {
      input_model.objective().priority()};
  for (auto const& [id, obj] : input_model.auxiliary_objectives()) {
    auto const prio = obj.priority();
    if (!prios.insert(prio).second) {
      return ortools::InvalidArgumentErrorBuilder()
             << "repeated objective priority: " << prio;
    }
    OR_RETURN_IF_ERROR(AddObjective(obj, id, true));
  }
  OR_RETURN_IF_ERROR(AddSOS(input_model.sos1_constraints(), true));
  OR_RETURN_IF_ERROR(AddSOS(input_model.sos2_constraints(), false));
  OR_RETURN_IF_ERROR(AddIndicators(input_model.indicator_constraints()));
  OR_RETURN_IF_ERROR(
      AddQuadraticConstraints(input_model.quadratic_constraints()));
  OR_RETURN_IF_ERROR(AddSecondOrderConeConstraints(
      input_model.second_order_cone_constraints()));
  return absl::OkStatus();
}

absl::Status XpressSolver::AddNewVariables(
    const VariablesProto& new_variables) {
  OR_ASSIGN_OR_RETURN(const int num_old_variables,
                      xpress_->GetIntAttr(XPRS_ORIGINALCOLS));
  const int num_new_variables = new_variables.lower_bounds().size();
  std::vector<char> variable_type(num_new_variables);
  OR_ASSIGN_OR_RETURN(int const n_variables,
                      xpress_->GetIntAttr(XPRS_ORIGINALCOLS));
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
    // save the call to XPRSchgcoltype() in AddVars()
    variable_type.clear();
  }
  OR_RETURN_IF_ERROR(
      xpress_->AddVars(num_new_variables, {}, new_variables.lower_bounds(),
                       new_variables.upper_bounds(), variable_type));

  if (extract_names_) {
    OR_RETURN_IF_ERROR(AddNames(xpress_.get(), XPRS_NAMES_COLUMN,
                                num_old_variables, 0, num_new_variables,
                                new_variables));
  }

  return absl::OkStatus();
}

XpressSolver::XpressSolver(std::unique_ptr<Xpress> g_xpress, bool extract_names)
    : xpress_(std::move(g_xpress)), extract_names_(extract_names) {}

void XpressSolver::ExtractBounds(double lb, double ub, char& sense, double& rhs,
                                 double& rng) {
  sense = XPRS_EQUAL;
  rhs = 0.0;
  rng = 0.0;
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
}

absl::Status XpressSolver::AddNewLinearConstraints(
    const LinearConstraintsProto& constraints) {
  // TODO: we might be able to improve performance by setting coefs also
  OR_ASSIGN_OR_RETURN(int const num_old_constraints,
                      xpress_->GetIntAttr(XPRS_ORIGINALROWS));
  const int num_new_constraints = constraints.lower_bounds().size();
  std::vector<char> constraint_sense;
  constraint_sense.reserve(num_new_constraints);
  std::vector<double> constraint_rhs;
  constraint_rhs.reserve(num_new_constraints);
  std::vector<double> constraint_rng;
  constraint_rng.reserve(num_new_constraints);
  OR_ASSIGN_OR_RETURN(int n_constraints,
                      xpress_->GetIntAttr(XPRS_ORIGINALROWS));
  for (int i = 0; i < num_new_constraints; ++i) {
    const int64_t id = constraints.ids(i);
    LinearConstraintData& constraint_data =
        gtl::InsertKeyOrDie(&linear_constraints_map_, id);
    constraint_data.lower_bound = constraints.lower_bounds(i);
    constraint_data.upper_bound = constraints.upper_bounds(i);
    constraint_data.constraint_index = i + n_constraints;
    char sense = XPRS_EQUAL;
    double rhs = 0.0;
    double rng = 0.0;
    ExtractBounds(constraint_data.lower_bound, constraint_data.upper_bound,
                  sense, rhs, rng);
    constraint_sense.emplace_back(sense);
    constraint_rhs.emplace_back(rhs);
    constraint_rng.emplace_back(rng);
  }
  // Add all constraints in one call.
  OR_RETURN_IF_ERROR(
      xpress_->AddConstrs(constraint_sense, constraint_rhs, constraint_rng));
  if (extract_names_) {
    OR_RETURN_IF_ERROR(AddNames(xpress_.get(), XPRS_NAMES_ROW,
                                num_old_constraints, 0, num_new_constraints,
                                constraints));
  }
  return absl::OkStatus();
}

absl::Status XpressSolver::AddObjective(
    const ObjectiveProto& objective,
    std::optional<AuxiliaryObjectiveId> objective_id, bool multiobj) {
  double weight = 1.0;
  bool haveId = objective_id.has_value();

  if (multiobj) {
    // In ortools smaller priority means more important, in Xpress,
    // higher priority means more important, so we must invert priorities.
    // Moreover, in Xpress priorities are 32bit.
    // Note that ortools does not allow duplicate priorities, this is checked
    // by the caller.
    if (objective.priority() <= INT_MIN || objective.priority() > INT_MAX) {
      return ortools::InvalidArgumentErrorBuilder()
             << "Xpress only supports 32bit signed integers as objective "
                "priority, not "
             << objective.priority();
    }
  }

  // Set/adjust objective sense.
  if (!multiobj) {
    // Not a multi-objective model
    OR_RETURN_IF_ERROR(xpress_->SetObjectiveSense(objective.maximize()));
  } else if (!objective_id.has_value()) {
    // First objective in multi-objective.
    OR_RETURN_IF_ERROR(xpress_->SetObjectiveSense(objective.maximize()));
    is_multiobj_ = true;
  } else {
    // Auxiliary objective in multi-objective. Xpress does not support
    // different objective senses for different objectives. So if the sense
    // does not match we set the weight to -1.0 to invert the objective
    // coefficients.
    OR_ASSIGN_OR_RETURN(double const objsen,
                        xpress_->GetDoubleAttr(XPRS_OBJSENSE));
    if (objective.maximize() != (objsen < 0.0)) {
      weight = -1.0;
    }
  }

  // Extract the objective.
  // First do quadratic terms since these are illegal for auxiliary objectives
  // Quadratic terms
  const int num_terms = objective.quadratic_coefficients().row_ids().size();
  if (num_terms > 0) {
    if (multiobj && objective_id.has_value()) {
      return ortools::InvalidArgumentErrorBuilder()
             << "Xpress does not support quadratic terms in anything but the "
                "first objective";
    }
    std::vector<int> first_var_index(num_terms);
    std::vector<int> second_var_index(num_terms);
    std::vector<double> coefficients(num_terms);
    for (int k = 0; k < num_terms; ++k) {
      const int64_t row_id = objective.quadratic_coefficients().row_ids(k);
      const int64_t column_id =
          objective.quadratic_coefficients().column_ids(k);
      first_var_index[k] = or2xprs(variables_map_, row_id);
      second_var_index[k] = or2xprs(variables_map_, column_id);
      // XPRESS supposes a 1/2 implicit multiplier to quadratic terms (see doc)
      // We have to multiply it by 2 for diagonal terms
      double m = first_var_index[k] == second_var_index[k] ? 2 : 1;
      coefficients[k] = objective.quadratic_coefficients().coefficients(k) * m;
    }
    OR_RETURN_IF_ERROR(xpress_->SetQuadraticObjective(
        first_var_index, second_var_index, coefficients));
  }

  // Linear terms
  std::vector<int> index;
  index.reserve(objective.linear_coefficients().ids_size());
  for (const int64_t id : objective.linear_coefficients().ids()) {
    index.push_back(or2xprs(variables_map_, id));
  }

  if (multiobj) {
    if (!objective_id.has_value()) {
      // Primary objective
      OR_RETURN_IF_ERROR(xpress_->SetLinearObjective(
          objective.offset(), index, objective.linear_coefficients().values()));
      OR_RETURN_IF_ERROR(xpress_->SetObjectiveIntControl(
          0, XPRS_OBJECTIVE_PRIORITY,
          // checked above
          static_cast<int>(-objective.priority())));
      OR_RETURN_IF_ERROR(
          xpress_->SetObjectiveDoubleControl(0, XPRS_OBJECTIVE_WEIGHT, weight));
    } else {
      // Auxiliary objective
      OR_ASSIGN_OR_RETURN(
          int const newid,
          xpress_->AddObjective(
              objective.offset(), static_cast<int>(index.size()),
              absl::MakeSpan(index), objective.linear_coefficients().values(),
              // checked above
              static_cast<int>(-objective.priority()), weight));
      gtl::InsertOrDie(&objectives_map_, objective_id.value(), newid);
    }
  } else {
    OR_RETURN_IF_ERROR(xpress_->SetLinearObjective(
        objective.offset(), index, objective.linear_coefficients().values()));
  }

  return absl::OkStatus();
}

/** Add an SOS constraint.
 * Note that in ortools an SOS constraint is made up from expressions and not
 * just variables. Here, we only support SOSs in which each expression is just
 * a single variable with coefficient 1.
 * If we wanted to support expressions as well we would have to
 * - for each element in the SOS that is an expression introduce an auxiliary
 *   variable x_aux = expression
 * - construct the SOS constraint on the auxiliary variables instead.
 * These auxiliary variables would have been maintained during model updates,
 * would need special handling during solution processing etc. At the moment
 * we do not implement that and instead require the user to create these
 * auxiliaries at the ortools level.
 *
 * Also, ortools supports SOSs with identical elements and assumes that
 * something like { x, x } is reduced to just { x }. This is debatable:
 * If you consider { x, x } a set, then clearly it is the same as { x }.
 * But if you consider "at most one of x and x may be non-zero", then { x, x }
 * implies x=0. This is not how ortools interprets SOSs with duplicate entries
 * (see the tests).
 * We do not check for duplicate entries here, but Xpress will choke on them.
 */
absl::Status XpressSolver::AddSOS(
    const google::protobuf::Map<AnyConstraintId, SosConstraintProto>& sets,
    bool sos1) {
  if (sets.empty()) return absl::OkStatus();
  std::vector<XPRSint64> start;
  std::vector<int> colind;
  std::vector<double> refval;
  OR_ASSIGN_OR_RETURN(int nextId, xpress_->GetIntAttr(XPRS_ORIGINALSETS));
  int const num_old_sets = nextId;
  auto* sosmap = sos1 ? &sos1_map_ : &sos2_map_;
  for (auto const& [sosId, sos] : sets) {
    start.push_back(colind.size());
    auto count = sos.expressions_size();
    bool const has_weight = sos.weights_size() > 0;
    for (decltype(count) i = 0; i < count; ++i) {
      auto const& expr = sos.expressions(i);
      double const weight = has_weight ? sos.weights(i) : (i + 1);
      // Note: A constant value in an SOS forces all others to zero. At the
      //       moment we do not support this. We consider this an edge case.
      OR_ASSIGN_OR_RETURN(std::optional<VarId> x,
                          ExtractSingleton(expr, SingletonType::SOS, nullptr));
      colind.push_back(or2xprs(variables_map_, x.value()));
      refval.push_back(weight);
    }
    gtl::InsertOrDie(sosmap, sosId, nextId);
    ++nextId;
  }
  std::vector<char> settype(start.size(), sos1 ? '1' : '2');
  OR_RETURN_IF_ERROR(xpress_->AddSets(settype, start, colind, refval));
  if (extract_names_) {
    OR_RETURN_IF_ERROR(AddNames(xpress_.get(), XPRS_NAMES_SET, num_old_sets,
                                sets.begin(), sets.end(), sets));
  }
  return absl::OkStatus();
}

void XpressSolver::ExtractLinear(SparseDoubleVectorProto const& expr,
                                 std::vector<int>& colind,
                                 std::vector<double>& coef) {
  // Note: Constant terms in expressions are already mixed into the
  //       right-hand side by ortools, so we don't have to deal with them
  //       here.
  auto terms = expr.ids_size();
  colind.reserve(colind.size() + terms);
  coef.reserve(coef.size() + terms);
  for (decltype(terms) i = 0; i < terms; ++i) {
    colind.push_back(or2xprs(variables_map_, expr.ids(i)));
    coef.push_back(expr.values(i));
  }
}

void XpressSolver::ExtractQuadratic(QuadraticConstraintProto const& expr,
                                    std::vector<int>& lin_colind,
                                    std::vector<double>& lin_coef,
                                    std::vector<int>& quad_col1,
                                    std::vector<int>& quad_col2,
                                    std::vector<double>& quad_coef) {
  // Note: Constant terms in expressions are already mixed into the
  //       right-hand side by ortools, so we don't have to deal with them
  //       here.
  auto const& lin = expr.linear_terms();
  auto linTerms = lin.ids_size();
  lin_colind.reserve(lin_colind.size() + linTerms);
  lin_coef.reserve(lin_coef.size() + linTerms);
  for (decltype(linTerms) i = 0; i < linTerms; ++i) {
    lin_colind.push_back(or2xprs(variables_map_, lin.ids(i)));
    lin_coef.push_back(lin.values(i));
  }
  auto const& quad = expr.quadratic_terms();
  auto quadTerms = quad.row_ids_size();
  quad_col1.reserve(quad_col1.size() + quadTerms);
  quad_col2.reserve(quad_col2.size() + quadTerms);
  quad_coef.reserve(quad_coef.size() + quadTerms);
  for (decltype(quadTerms) i = 0; i < quadTerms; ++i) {
    int const col1 = or2xprs(variables_map_, quad.row_ids(i));
    int const col2 = or2xprs(variables_map_, quad.column_ids(i));
    double coef = quad.coefficients(i);
    if (col1 != col2) coef *= 0.5;
    quad_col1.push_back(col1);
    quad_col2.push_back(col2);
    quad_coef.push_back(coef);
  }
  /** TODO: How do we handle constant terms in expressions? */
}

absl::Status XpressSolver::AddIndicators(
    const google::protobuf::Map<IndicatorConstraintId,
                                IndicatorConstraintProto>& indicators) {
  if (indicators.empty()) return absl::OkStatus();
  // For XPRSaddrows()
  size_t count = indicators.size();
  std::vector<double> rhs(count);
  std::vector<double> rng(count);
  std::vector<char> sense(count);
  std::vector<XPRSint64> start(count);
  std::vector<int> colind;
  std::vector<double> rowcoef;
  // For XPRSsetindicators()
  std::vector<int> i_rowind(count);
  std::vector<int> i_colind(count);
  std::vector<int> i_complement(count);
  OR_ASSIGN_OR_RETURN(int const oldRows,
                      xpress_->GetIntAttr(XPRS_ORIGINALROWS));
  int min_icol = std::numeric_limits<int>::max();
  int max_icol = std::numeric_limits<int>::min();
  bool check_types = false;
  int next = 0;
  for (auto const& [ortoolsId, indicator] : indicators) {
    start[next] = colind.size();
    ExtractBounds(indicator.lower_bound(), indicator.upper_bound(), sense[next],
                  rhs[next], rng[next]);
    // ortools tests require us to raise an error on ranged indicator
    // constraints
    if (sense[next] == XPRS_RANGE) {
      return ortools::InvalidArgumentErrorBuilder()
             << "indicator constraint on ranged constraint";
    }

    ExtractLinear(indicator.expression(), colind, rowcoef);

    i_rowind[next] = oldRows + next;
    if (indicator.has_indicator_id()) {
      i_colind[next] = or2xprs(variables_map_, indicator.indicator_id());
      if (i_colind[next] < min_icol) min_icol = i_colind[next];
      if (i_colind[next] > max_icol) max_icol = i_colind[next];
      i_complement[next] = indicator.activate_on_zero() ? -1 : 1;
      check_types = true;
    } else {
      // By definition, this is an inactive constraint, see
      // indicator_constraint.h
      i_colind[next] = -1;
      i_complement[next] = 0;
      sense[next] = XPRS_NONBINDING;
    }
    LinearConstraintData& data =
        gtl::InsertKeyOrDie(&indicator_map_, ortoolsId);
    data.constraint_index = oldRows + next;
    data.lower_bound = indicator.lower_bound();
    data.upper_bound = indicator.upper_bound();
    ++next;
  }
  if (check_types) {
    // We have at least one active indicator constraint. We must check types
    // of variables. If they are integer but within the range of a binary
    // then we must convert them to a binary, otherwise Xpress will reject
    // them.
    std::vector<double> orig_lb(1 + max_icol - min_icol);
    std::vector<double> orig_ub(1 + max_icol - min_icol);
    std::vector<char> orig_type(1 + max_icol - min_icol);
    OR_RETURN_IF_ERROR(
        xpress_->GetLB(absl::MakeSpan(orig_lb), min_icol, max_icol));
    OR_RETURN_IF_ERROR(
        xpress_->GetUB(absl::MakeSpan(orig_ub), min_icol, max_icol));
    OR_RETURN_IF_ERROR(
        xpress_->GetColType(absl::MakeSpan(orig_type), min_icol, max_icol));
    std::vector<int> colind_bnd;
    std::vector<double> new_bds;
    std::vector<char> new_bdtype;
    std::vector<int> colind_type;
    std::vector<char> new_type;
    for (auto i : i_colind) {
      if (i < 0) continue;
      int const idx = i - min_icol;
      switch (orig_type[idx]) {
        case XPRS_BINARY:
          // Ok, nothing to do.
          break;
        case XPRS_INTEGER:
          // Convert to binary if within range.
          if (orig_lb[idx] >= 0.0 && orig_lb[idx] <= 1.0 &&
              orig_ub[idx] >= 0.0 && orig_ub[idx] <= 1.0) {
            double const l = ceil(orig_lb[idx]);
            double const u = floor(orig_ub[idx]);
            orig_lb[idx] = l;  // In case variable is indicator more than once
            orig_ub[idx] = u;
            // It would require less storage if we performed two calls to
            // XPRSchgbounds(): one for changing all lower bounds and one for
            // changing all upper bounds. However, this may temporarily result
            // in conflicting bounds, which Xpress does not support. So we must
            // change all bounds in one single call and collect appropriate data
            // for that.
            colind_bnd.push_back(i);
            colind_bnd.push_back(i);
            new_bds.push_back(l);
            new_bds.push_back(u);
            new_bdtype.push_back('L');
            new_bdtype.push_back('U');
            colind_type.push_back(i);
            new_type.push_back(XPRS_BINARY);
          } else {
            nonbinary_indicator_ = true;
          }
          break;
        default:
          // Anything else cannot be used as indicator variable.
          nonbinary_indicator_ = true;
          break;
      }
    }
    // Change column type and bounds. Note that we must first change the type
    // since changing the type to 'B' will automatically change bounds to [0,1].
    // After that we can fix up the bounds.
    if (colind_type.size()) {
      OR_RETURN_IF_ERROR(xpress_->ChgColType(colind_type, new_type));
    }
    if (colind_bnd.size()) {
      OR_RETURN_IF_ERROR(xpress_->ChgBounds(colind_bnd, new_bdtype, new_bds));
    }
  }
  OR_RETURN_IF_ERROR(xpress_->AddRows(sense, rhs, rng, start, colind, rowcoef));
  OR_RETURN_IF_ERROR(xpress_->SetIndicators(i_rowind, i_colind, i_complement));
  return absl::OkStatus();
}

absl::Status XpressSolver::AddQuadraticConstraints(
    const google::protobuf::Map<QuadraticConstraintId,
                                QuadraticConstraintProto>& constraints) {
  std::vector<int> lin_colind;
  std::vector<double> lin_coef;
  std::vector<int> quad_col1;
  std::vector<int> quad_col2;
  std::vector<double> quad_coef;
  OR_ASSIGN_OR_RETURN(int next, xpress_->GetIntAttr(XPRS_ORIGINALROWS));
  for (const auto& [ortoolsId, quad] : constraints) {
    // Xpress has no function to add multiple quadratic rows in one shot, so we
    // add the linear part one by one as well.
    char sense;
    double rhs;
    double rng;
    ExtractBounds(quad.lower_bound(), quad.upper_bound(), sense, rhs, rng);
    lin_colind.clear();
    lin_coef.clear();
    quad_col1.clear();
    quad_col2.clear();
    quad_coef.clear();
    ExtractQuadratic(quad, lin_colind, lin_coef, quad_col1, quad_col2,
                     quad_coef);
    OR_RETURN_IF_ERROR(xpress_->AddQRow(sense, rhs, rng, lin_colind, lin_coef,
                                        quad_col1, quad_col2, quad_coef));
    LinearConstraintData& data =
        gtl::InsertKeyOrDie(&quad_constraints_map_, ortoolsId);
    data.constraint_index = next;
    data.lower_bound = quad.lower_bound();
    data.upper_bound = quad.upper_bound();
    ++next;
  }
  return absl::OkStatus();
}

// Extract second order cone constraints.
// Note that we only support
//   sum(i in I) x_i^2 <= x_0^2
absl::Status XpressSolver::AddSecondOrderConeConstraints(
    const google::protobuf::Map<SecondOrderConeConstraintId,
                                SecondOrderConeConstraintProto>& constraints) {
  std::vector<int> cols;
  std::vector<double> coefs;
  OR_ASSIGN_OR_RETURN(int next, xpress_->GetIntAttr(XPRS_ORIGINALROWS));
  for (auto const& [ortoolsId, soc] : constraints) {
    cols.clear();
    coefs.clear();
    double rhs = 0.0;
    auto const& ub = soc.upper_bound();
    double coef;
    OR_ASSIGN_OR_RETURN(std::optional<VarId> const x0,
                        ExtractSingleton(ub, SingletonType::SOCBound, &coef));
    if (x0.has_value()) {
      cols.push_back(or2xprs(variables_map_, x0.value()));
      coefs.push_back(-coef * coef);
    } else {
      rhs = coef * coef;
    }

    for (auto const& arg : soc.arguments_to_norm()) {
      OR_ASSIGN_OR_RETURN(std::optional<VarId> const x,
                          ExtractSingleton(arg, SingletonType::SOCNorm, &coef));
      cols.push_back(or2xprs(variables_map_, x.value()));
      coefs.push_back(coef * coef);
    }
    OR_RETURN_IF_ERROR(
        xpress_->AddQRow('L', rhs, 0.0, {}, {}, cols, cols, coefs));
    LinearConstraintData& data = gtl::InsertKeyOrDie(&soc_map_, ortoolsId);
    data.constraint_index = next;
    data.lower_bound = kMinusInf;
    data.upper_bound = 0.0;
    ++next;
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
    row_index.push_back(or2xprs(linear_constraints_map_, matrix.row_ids(k)));
    col_index.push_back(or2xprs(variables_map_, matrix.column_ids(k)));
  }
  return xpress_->ChgCoeffs(row_index, col_index, matrix.coefficients());
}

absl::StatusOr<SolveResultProto> XpressSolver::Solve(
    const SolveParametersProto& parameters,
    const ModelSolveParametersProto& model_parameters,
    MessageCallback message_callback,
    const CallbackRegistrationProto& callback_registration, Callback callback,
    const SolveInterrupter* interrupter) {
  force_postsolve_ = false;
  primal_sol_avail_ = XPRS_SOLAVAILABLE_NOTFOUND;
  dual_sol_avail_ = XPRS_SOLAVAILABLE_NOTFOUND;
  solvestatus_ = XPRS_SOLVESTATUS_UNSTARTED;
  solstatus_ = XPRS_SOLSTATUS_NOTFOUND;
  algorithm_ = XPRS_ALG_DEFAULT;
  OR_RETURN_IF_ERROR(ModelSolveParametersAreSupported(
      model_parameters, kXpressSupportedStructures, "XPRESS"));
  OR_ASSIGN_OR_RETURN(is_mip_, xpress_->IsMIP());
  const absl::Time start = absl::Now();

  OR_RETURN_IF_ERROR(CheckRegisteredCallbackEvents(
      callback_registration,
      is_mip_ ? SupportedMIPEvents_ : SupportedLPEvents_));

  // Check that bounds are not inverted just before solve
  // XPRESS returns "infeasible" when bounds are inverted
  {
    OR_ASSIGN_OR_RETURN(const InvertedBounds inverted_bounds,
                        ListInvertedBounds());
    OR_RETURN_IF_ERROR(inverted_bounds.ToStatus());
  }
  // Check that we don't have non-binary indicator variables
  if (nonbinary_indicator_) {
    return ortools::InvalidArgumentErrorBuilder()
           << "indicator variable is not binary";
  }

  // Register callbacks and create scoped context to automatically if an
  // exception has been thrown during optimization.
  ScopedSolverContext solveContext(xpress_.get());
  OR_RETURN_IF_ERROR(solveContext.AddCallbacks(message_callback,
                                               callback_registration, callback,
                                               interrupter, variables_map_));
  std::string export_model = "";
  OR_RETURN_IF_ERROR(
      solveContext.ApplyParameters(parameters, message_callback, &export_model,
                                   &force_postsolve_, &stop_after_lp_));
  OR_RETURN_IF_ERROR(solveContext.ApplyModelParameters(
      model_parameters, variables_map_, linear_constraints_map_,
      objectives_map_));

  // We are ready to solve the problem. If we are asked to export the
  // problem, then do that now. Depending on the file name extension we
  // either create a save file or an LP/MPS file.
  if (export_model.length() > 0) {
    if (export_model.length() >= 4 &&
        export_model.compare(export_model.length() - 4, 4, ".svf") == 0) {
      OR_RETURN_IF_ERROR(xpress_->SaveAs(export_model.c_str()));
    } else {
      OR_RETURN_IF_ERROR(xpress_->WriteProb(export_model.c_str()));
    }
  }

  // Solve. We use the generic XPRSoptimize() and let Xpress decide what is
  // the best algorithm. Note that we do not pass flags to the function
  // either. We assume that algorithms are configured via controls like
  // LPFLAGS.
  OR_RETURN_IF_ERROR(
      xpress_->Optimize(stop_after_lp_ ? "l" : "", &solvestatus_, &solstatus_));
  // Reraise any exception now. Note that we cannot just limit the scope of
  // solveContext since its destructor will restore controls settings.
  // On the other hand, when fetching results we need to check some controls
  // (for example, BARALG to decide whether we need to report barrier or
  // first order iterations).
  OR_RETURN_IF_ERROR(solveContext.HandleCallbackProblems());
  OR_RETURN_IF_ERROR(
      xpress_->GetSolution(&primal_sol_avail_, std::nullopt, 0, -1));
  OR_RETURN_IF_ERROR(xpress_->GetDuals(&dual_sol_avail_, std::nullopt, 0, -1));
  OR_ASSIGN_OR_RETURN(algorithm_, xpress_->GetIntAttr(XPRS_ALGORITHM));
  OR_ASSIGN_OR_RETURN(optimizetypeused_,
                      xpress_->GetIntAttr(XPRS_OPTIMIZETYPEUSED));
  // Do NOT postsolve by default here!
  // All functions we use operate in the original space
  // and postsolving here is harmful if we want to come back and solve with
  // an extended time limit, for example. We defer postsolve until the latest
  // point possible. This means we call it in ::Update() and
  // ::ComputeInfeasibleSubsystem()
  if (force_postsolve_)
    OR_RETURN_IF_ERROR(xpress_->PostSolve()) << "XPRSpostsolve() failed";

  OR_ASSIGN_OR_RETURN(
      SolveResultProto solve_result,
      ExtractSolveResultProto(start, model_parameters, parameters));

  // Other solvers reset/clear model_parameters here.
  // We don't do this. Consider for example branching priorities. These
  // can only be changed if the model is not in presolved state (see the
  // reference documentation of XPRSloaddirs()).
  // If the solve terminated due to a resource limit then the model will still
  // be in presolved state. In order to clear branching priorities we would
  // have to XPRSpostsolve() the problem. That would mean that the next call
  // to Solve() would solve the model from scratch. Since this would make
  // incremental solves impossible, we don't clear model_parameters here.

  return solve_result;
}

absl::StatusOr<SolveResultProto> XpressSolver::ExtractSolveResultProto(
    absl::Time start, const ModelSolveParametersProto& model_parameters,
    const SolveParametersProto& solve_parameters) {
  SolveResultProto result;
  OR_RETURN_IF_ERROR(
      AppendSolution(result, model_parameters, solve_parameters));
  OR_ASSIGN_OR_RETURN(*result.mutable_solve_stats(), GetSolveStats(start));
  OR_ASSIGN_OR_RETURN(const double best_primal_bound, GetBestPrimalBound());
  OR_ASSIGN_OR_RETURN(const double best_dual_bound, GetBestDualBound());
  OR_ASSIGN_OR_RETURN(
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
  OR_ASSIGN_OR_RETURN(double const objsen,
                      xpress_->GetDoubleAttr(XPRS_OBJSENSE));
  return objsen * kPlusInf;
}

absl::StatusOr<double> XpressSolver::GetBestDualBound() const {
  if (is_mip_) {
    return xpress_->GetDoubleAttr(XPRS_BESTBOUND);
  }
  // Xpress does not have an attribute to report the best dual bound from
  // simplex
  else {
    OR_ASSIGN_OR_RETURN(int const alg, xpress_->GetIntAttr(XPRS_ALGORITHM));
    if (alg == XPRS_ALG_BARRIER)
      return xpress_->GetDoubleAttr(XPRS_BARDUALOBJ);
    else if (primal_sol_avail_ == XPRS_SOLAVAILABLE_OPTIMAL)
      return xpress_->GetDoubleAttr(XPRS_OBJVAL);
  }
  // No dual bound available, return infinity.
  OR_ASSIGN_OR_RETURN(double const objsen,
                      xpress_->GetDoubleAttr(XPRS_OBJSENSE));
  return objsen * kMinusInf;
}

/** Extend a solution with multi-objective information (if there is more than
 * one objective).
 */
absl::Status XpressSolver::ExtendWithMultiobj(SolutionProto& solution) {
  // We may not have solved for all objectives, so make sure we query only
  // those that were solved.
  OR_ASSIGN_OR_RETURN(int const nSolved, xpress_->GetIntAttr(XPRS_SOLVEDOBJS));
  auto* objvals =
      solution.mutable_primal_solution()->mutable_auxiliary_objective_values();
  for (auto const& [ortoolsId, xpressId] : objectives_map_) {
    OR_ASSIGN_OR_RETURN(double const thisobj,
                        xpress_->CalculateObjectiveN(xpressId, nullptr));
    (*objvals)[ortoolsId] = thisobj;
  }
  return absl::OkStatus();
}

absl::Status XpressSolver::AppendSolution(
    SolveResultProto& solve_result,
    const ModelSolveParametersProto& model_parameters,
    const SolveParametersProto& solve_parameters) {
  OR_ASSIGN_OR_RETURN(int const nVars, xpress_->GetIntAttr(XPRS_ORIGINALCOLS));
  if (is_mip_) {
    std::vector<double> x(nVars);
    int avail;
    OR_RETURN_IF_ERROR(
        xpress_->GetSolution(&avail, absl::MakeSpan(x), 0, nVars - 1));
    if (avail != XPRS_SOLAVAILABLE_NOTFOUND) {
      SolutionProto solution{};
      solution.mutable_primal_solution()->set_feasibility_status(
          getPrimalSolutionStatus());
      OR_ASSIGN_OR_RETURN(const double objval,
                          xpress_->GetDoubleAttr(XPRS_OBJVAL));
      solution.mutable_primal_solution()->set_objective_value(objval);
      OR_RETURN_IF_ERROR(ExtendWithMultiobj(solution));
      XpressVectorToSparseDoubleVector(
          x, variables_map_,
          *solution.mutable_primal_solution()->mutable_variable_values(),
          model_parameters.variable_values_filter());
      *solve_result.add_solutions() = std::move(solution);
    }
  } else {
    // Fetch all results from XPRESS
    OR_ASSIGN_OR_RETURN(int const nCons,
                        xpress_->GetIntAttr(XPRS_ORIGINALROWS));
    std::vector<double> primals(nVars);
    std::vector<double> duals(nCons);
    std::vector<double> reducedCosts(nVars);

    // This is for handling an edge case:
    // If an LP solve is interrupted then XPRSgetsolution() and friends will
    // return "not available". However, there may still be a current
    // primal or dual feasible solution available - depending on the algorithm.
    // Users and ortools tests may expect these to be returned in some cases,
    // so we try to pick them up. This must be done via XPRSgetlpsol() which
    // is designed for exactly this edge case.
    // This only applies to LPs.
    auto hasSolution =
        (optimizetypeused_ ==
         0) &&  // 0 = LP, 1 = MIP, 2/3 = nonlin local/global
        xpress_
            // Note: XPRSgetlpsol() returns solution in original space
            ->GetLpSol(absl::MakeSpan(primals), absl::MakeSpan(duals),
                       absl::MakeSpan(reducedCosts))
            .ok();

    SolutionProto solution{};
    bool storeSolutions = (solvestatus_ == XPRS_SOLVESTATUS_STOPPED ||
                           solvestatus_ == XPRS_SOLVESTATUS_COMPLETED);

    if (isPrimalFeasible()) {
      // The preferred methods for obtaining primal information are
      // XPRSgetsolution() and XPRSgetslacks() (not used here)
      // XPRSgetsolution() returns solution in original space.
      OR_RETURN_IF_ERROR(
          xpress_->GetSolution(nullptr, absl::MakeSpan(primals), 0, nVars - 1));
      solution.mutable_primal_solution()->set_feasibility_status(
          getPrimalSolutionStatus());
      OR_ASSIGN_OR_RETURN(const double primalBound, GetBestPrimalBound());
      solution.mutable_primal_solution()->set_objective_value(primalBound);
      XpressVectorToSparseDoubleVector(
          primals, variables_map_,
          *solution.mutable_primal_solution()->mutable_variable_values(),
          model_parameters.variable_values_filter());
      OR_RETURN_IF_ERROR(ExtendWithMultiobj(solution));
    } else if (storeSolutions) {
      // Even if we are not primal feasible, store the results we obtained
      // from XPRSgetlpsolution(). The feasibility status of this vector
      // is undetermined, though.
      solution.mutable_primal_solution()->set_feasibility_status(
          SOLUTION_STATUS_UNDETERMINED);
      OR_ASSIGN_OR_RETURN(const double primalBound, GetBestPrimalBound());
      solution.mutable_primal_solution()->set_objective_value(primalBound);
      XpressVectorToSparseDoubleVector(
          primals, variables_map_,
          *solution.mutable_primal_solution()->mutable_variable_values(),
          model_parameters.variable_values_filter());
    }

    if (isDualFeasible()) {
      // The preferred methods for obtain dual information are XPRSgetduals()
      // and XPRSgetredcosts().
      // XPRSgetduals() and XPRSgetredcosts() both return values in the
      // original space.
      OR_RETURN_IF_ERROR(
          xpress_->GetDuals(nullptr, absl::MakeSpan(duals), 0, nCons - 1));
      OR_RETURN_IF_ERROR(xpress_->GetRedCosts(
          nullptr, absl::MakeSpan(reducedCosts), 0, nVars - 1));
      solution.mutable_dual_solution()->set_feasibility_status(
          getDualSolutionStatus());
      OR_ASSIGN_OR_RETURN(const double dualBound, GetBestDualBound());
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
      OR_ASSIGN_OR_RETURN(const double dualBound, GetBestDualBound());
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
    OR_ASSIGN_OR_RETURN(auto basis, GetBasisIfAvailable(solve_parameters));
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

absl::StatusOr<std::optional<BasisProto>> XpressSolver::GetBasisIfAvailable(
    const SolveParametersProto&) {
  std::vector<int> xprs_variable_basis_status;
  std::vector<int> xprs_constraint_basis_status;
  // XPRSgetbasis() always returns values in the original space
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
    OR_ASSIGN_OR_RETURN(simplex_iters, xpress_->GetIntAttr(XPRS_SIMPLEXITER));
    OR_ASSIGN_OR_RETURN(barrier_iters, xpress_->GetIntAttr(XPRS_BARITER));
  } else if (algorithm_ == XPRS_ALG_DUAL || algorithm_ == XPRS_ALG_PRIMAL ||
             algorithm_ == XPRS_ALG_NETWORK) {
    // Definitely simplex
    OR_ASSIGN_OR_RETURN(simplex_iters, xpress_->GetIntAttr(XPRS_SIMPLEXITER));
  } else if (algorithm_ == XPRS_ALG_BARRIER) {
    // Barrier or first order
    OR_ASSIGN_OR_RETURN(const int baralg, xpress_->GetIntControl(XPRS_BARALG));
    if (baralg == 4) {
      OR_ASSIGN_OR_RETURN(first_order_iters, xpress_->GetIntAttr(XPRS_BARITER));
    } else {
      OR_ASSIGN_OR_RETURN(barrier_iters, xpress_->GetIntAttr(XPRS_BARITER));
    }
  }
  solve_stats.set_simplex_iterations(simplex_iters);
  solve_stats.set_barrier_iterations(barrier_iters);
  solve_stats.set_first_order_iterations(first_order_iters);
  if (is_mip_) {
    OR_ASSIGN_OR_RETURN(const int nodes, xpress_->GetIntAttr(XPRS_NODES));
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
  OR_ASSIGN_OR_RETURN(double const objsen,
                      xpress_->GetDoubleAttr(XPRS_OBJSENSE));
  OR_ASSIGN_OR_RETURN(int const stopStatus,
                      xpress_->GetIntAttr(XPRS_STOPSTATUS));
  bool const isMax = objsen < 0.0;
  bool checkSolStatus = false;

  if (!is_mip_) {
    // Handle some special LP termination reasons.
    OR_ASSIGN_OR_RETURN(int const lpstatus, xpress_->GetIntAttr(XPRS_LPSTATUS));
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
                isMax, LIMIT_TIME, best_dual_bound, "Time limit hit");
          case XPRS_STOP_CTRLC:  // fallthrough
          case XPRS_STOP_USER:
            return NoSolutionFoundTerminationProto(
                isMax, LIMIT_INTERRUPTED, best_dual_bound, "Interrupted");
          case XPRS_STOP_NODELIMIT:
            return NoSolutionFoundTerminationProto(
                isMax, LIMIT_NODE, best_dual_bound, "Node limit hit");
          case XPRS_STOP_ITERLIMIT:
            return NoSolutionFoundTerminationProto(
                isMax, LIMIT_ITERATION, best_dual_bound, "Node limit hit");
          case XPRS_STOP_WORKLIMIT:
            return NoSolutionFoundTerminationProto(
                isMax, LIMIT_OTHER, best_dual_bound, "Work limit hit");
          case XPRS_STOP_MEMORYERROR:
            // Despite its name, MEMORYERROR is not actually an error
            // but instead indicates that we hit a user defined memory
            // limit.
            return NoSolutionFoundTerminationProto(
                isMax, LIMIT_MEMORY, best_dual_bound, "Memory limit hit");
          default:
            if (stop_after_lp_) {
              // Stopping after the LP relaxation is treated as special
              // node limit
              return NoSolutionFoundTerminationProto(
                  isMax, LIMIT_NODE, best_dual_bound,
                  "Stopped after LP relaxation");
            } else {
              return TerminateForReason(isMax,
                                        TERMINATION_REASON_NO_SOLUTION_FOUND);
            }
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
            if (stop_after_lp_) {
              // Stopping after the LP relaxation is treated as special
              // node limit
              return FeasibleTerminationProto(
                  isMax, LIMIT_NODE, best_primal_bound, best_dual_bound,
                  "Stopped after LP relaxation");
            } else {
              return FeasibleTerminationProto(isMax, LIMIT_UNDETERMINED,
                                              best_primal_bound,
                                              best_dual_bound);
            }
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
  // We can only update if problem is not in presolved state.
  OR_RETURN_IF_ERROR(xpress_->PostSolve()) << "XPRSpostsolve() failed";
  return false;
}

absl::StatusOr<ComputeInfeasibleSubsystemResultProto>
XpressSolver::ComputeInfeasibleSubsystem(const SolveParametersProto& parameters,
                                         MessageCallback message_callback,
                                         const SolveInterrupter* interrupter) {
  OR_RETURN_IF_ERROR(xpress_->PostSolve()) << "XPRSpostsolve() failed";
  ScopedSolverContext solveContext(xpress_.get());
  OR_RETURN_IF_ERROR(
      solveContext.AddCallbacks(message_callback, CallbackRegistrationProto(),
                                nullptr, interrupter, variables_map_));
  OR_RETURN_IF_ERROR(solveContext.ApplyParameters(parameters, message_callback,
                                                  nullptr, nullptr, nullptr));

  return absl::UnimplementedError(
      "XpressSolver does not compute an infeasible subsystem (yet)");
}

absl::StatusOr<InvertedBounds> XpressSolver::ListInvertedBounds() const {
  InvertedBounds inverted_bounds;
  {
    OR_ASSIGN_OR_RETURN(const std::vector<double> var_lbs, xpress_->GetVarLb());
    OR_ASSIGN_OR_RETURN(const std::vector<double> var_ubs, xpress_->GetVarUb());
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
