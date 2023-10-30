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

// Provides a safe C++ interface for SCIP constraint handlers, which are
// described at https://www.scipopt.org/doc/html/CONS.php. For instructions to
// write a constraint handler, see the documentation of GScipConstraintHandler.
// Examples can be found in gscip_constraint_handler_test.cc.
#ifndef OR_TOOLS_GSCIP_GSCIP_CONSTRAINT_HANDLER_H_
#define OR_TOOLS_GSCIP_GSCIP_CONSTRAINT_HANDLER_H_

#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"
#include "ortools/gscip/gscip.h"
#include "ortools/gscip/gscip_callback_result.h"
#include "scip/type_cons.h"
#include "scip/type_sol.h"
#include "scip/type_var.h"

namespace operations_research {

// Properties for the constraint handler. It is recommended to set priorities
// and frequencies manually.
struct GScipConstraintHandlerProperties {
  // For members below, the corresponding SCIP constraint handler property name
  // is provided. See https://www.scipopt.org/doc/html/CONS.php#CONS_PROPERTIES
  // for details.
  //
  // While it is recommended to set your own parameters, we provide a default
  // set of parameters that has the following behavior:
  // * Enforcement and feasibility checking is done right after enforcing
  //   integrality, but before any other constraint handlers. This implies that
  //   it is only performed on integer solutions by default.
  // * Obsolete constraints are revisited every 100 nodes (eager frequency).
  //   This default follows the most common frequency in SCIP's existing
  //   constraint handlers.
  // * If separation is used, it is run before all constraint handlers and at
  //   every node. Note however that all separators are always run before any
  //   constraint handler separation. A user may control separation frequency
  //   either by changing this parameter or implementing a check in the
  //   callback.

  // Name of the constraint handler. See CONSHDLR_NAME in SCIP
  // documentation above.
  std::string name;

  // Description of the constraint handler. See CONSHDLR_DESC in SCIP
  // documentation above.
  std::string description;

  // Determines the order this constraint class is checked at each LP node. If
  // negative, the enforcement is only performed on integer solutions. See
  // CONSHDLR_ENFOPRIORITY in the SCIP documentation above. Only relevant if
  // enforcement callbacks are implemented.
  int enforcement_priority = -1;

  // Determines the order this constraint class runs in when testing solution
  // feasibility. If negative, the feasibility check is only performed on
  // integer solutions. See CONSHDLR_CHECKPRIORITY in the SCIP documentation
  // above. Only relevant if check callback is implemented.
  int feasibility_check_priority = -1;

  // Determines the order the separation from this constraint handler runs in
  // the cut loop. Note that separators are run before constraint handlers.
  // See CONSHDLR_SEPAPRIORITY in SCIP documentation above. Only relevant if
  // separation callbacks are implemented.
  int separation_priority = 3000000;

  // Frequency for separating cuts. See CONSHDLR_SEPAFREQ in the SCIP
  // documentation above. Only relevant if separation callbacks are implemented.
  int separation_frequency = 1;

  // Determines if this separator be delayed if another separator has already
  // found a cut. See CONSHDLR_DELAYSEPA in the SCIP documentation above.
  bool delay_separation = false;

  // Frequency for using all instead of only the useful constraints in
  // separation, propagation, and enforcement. For example, some constraints may
  // be aged out by SCIP if they are not relevant for several iterations. See
  // CONSHDLR_EAGERFREQ in SCIP documentation above.
  int eager_frequency = 100;

  // Indicates whether the constraint handler can be skipped if no constraints
  // from this handler are present in the model. In most cases, this should be
  // true. This should only be false for constraints that are not added
  // explicitly as a constraint, such as integrality. See CONSHDLR_NEEDSCONS in
  // SCIP documentation above.
  bool needs_constraints = true;
};

// Advanced use only. Indicates that if a variable moves in this direction, it
// can cause a constraint violation. `kBoth` is the safest option and always
// valid, but it is the least flexible for SCIP.
enum class RoundingLockDirection { kUp, kDown, kBoth };

// Options passed to SCIP when adding a cut.
struct GScipCutOptions {
  // Cut is only valid for the current subtree.
  bool local = false;
  // Cut is modifiable during node processing (subject to column generation).
  bool modifiable = false;
  // Cut can be removed from the LP due to aging or cleanup.
  bool removable = true;
  // Cut is forced to enter the LP.
  bool force_cut = false;
};

// Options passed to SCIP when adding a lazy constraint.
struct GScipLazyConstraintOptions {
  // Cut is only valid for the current subtree.
  bool local = false;
  // Constraint is subject to aging.
  bool dynamic = false;
};

struct GScipCallbackStats {
  // A unique id within a run, assigned consecutively by order of creation. -1
  // if no nodes have been created yet, or num_processed_nodes if
  // search is over. See SCIPgetCurrentNode().
  int64_t current_node_id = 0;

  // The number of processed nodes in the current run (i.e. does not include
  // nodes before a restart), including the focus node. See SCIPgetNNodes().
  int64_t num_processed_nodes = 0;

  // The total number of processed nodes in all runs, including the focus node.
  // If the solver restarts > 1 time, will be larger than
  // num_processed_nodes, otherwise is equal. See SCIPgetNTotalNodes().
  int64_t num_processed_nodes_total = 0;
};

// Interface to the callback context and underlying problem. Supports adding
// cuts and lazy constraints, and setting bounds. Prefer to use this context to
// query information instead of a raw SCIP pointer. Passed by value.
// TODO(user): Add support for branching.
class GScipConstraintHandlerContext {
 public:
  // Construct the context for the given handler. Following SCIP convention, if
  // SCIP_SOL is nullptr, then the current solution from the LP is used.
  GScipConstraintHandlerContext(GScip* gscip, const GScipCallbackStats* stats,
                                SCIP_CONSHDLR* current_handler,
                                SCIP_SOL* current_solution)
      : gscip_(gscip),
        stats_(stats),
        current_handler_(current_handler),
        current_solution_(current_solution) {}

  GScip* gscip() { return gscip_; }

  // Returns the current solution value of a variable. This may be for a given
  // solution (e.g. in CONS_SEPASOL) or the current LP/pseudosolution (e.g. in
  // CONS_SEPALP). Equivalent to calling SCIPgetSolVal.
  double VariableValue(SCIP_VAR* variable) const;

  // Adds a cut (row) to the SCIP separation storage.
  //
  // If this is called and succeeds, the callback result must be the one
  // returned or a higher priority result. The result returned is either kCutOff
  // (SCIP_CUTOFF) if SCIP determined that the cut results in infeasibility
  // based on local bounds, or kSeparated (SCIP_SEPARATED) otherwise.
  absl::StatusOr<GScipCallbackResult> AddCut(
      const GScipLinearRange& range, const std::string& name,
      const GScipCutOptions& options = GScipCutOptions());

  // Adds a lazy constraint as a SCIP linear constraint. This is similar to
  // adding it as a row (and it would be valid to add a lazy constraint with
  // AddCut and proper options), but it is treated as a higher-level object and
  // may affect other portions of SCIP such as propagation. This is a thin
  // wrapper on GScip::AddLinearConstraint() with different defaults.
  //
  // If this is called and succeeds, the callback result must be kConsAdded
  // (equivalent to SCIP_CONSADDED) or a higher priority result.
  absl::Status AddLazyLinearConstraint(
      const GScipLinearRange& range, const std::string& name,
      const GScipLazyConstraintOptions& options = GScipLazyConstraintOptions());

  // The functions below set variable bounds. If they are used to cut off a
  // solution, then the callback result must be kReducedDomain
  // (SCIP_REDUCEDDOM) or a higher priority result.

  absl::Status SetLocalVarLb(SCIP_VAR* var, double value);
  absl::Status SetLocalVarUb(SCIP_VAR* var, double value);
  absl::Status SetGlobalVarLb(SCIP_VAR* var, double value);
  absl::Status SetGlobalVarUb(SCIP_VAR* var, double value);

  double LocalVarLb(SCIP_VAR* var) const;
  double LocalVarUb(SCIP_VAR* var) const;
  double GlobalVarLb(SCIP_VAR* var) const;
  double GlobalVarUb(SCIP_VAR* var) const;

  const GScipCallbackStats& stats() const { return *stats_; }

 private:
  GScip* gscip_;
  const GScipCallbackStats* stats_;
  SCIP_CONSHDLR* current_handler_;
  SCIP_SOL* current_solution_;
};

// Constraint handler class. To implement a constraint handler, the user can
// inherit this class and implement the desired callback functions. The
// templated ConstraintData is the equivalent of SCIP's SCIP_CONSHDLRDATA, and
// can hold the data needed for the constraint. To then use it, the function
// Register must be called once, and AddCallbackConstraint must be called for
// each constraint to be added in this constraint handler.
//
// There is a one-to-one mapping between relevant SCIP callback functions and
// the functions in this class; see SCIP documentation for which types of
// callbacks to use. Make sure to follow SCIP's rules (e.g. if implementing
// enforcement, all enforcement and check callbacks must be implemented).
//
// For examples of usage, see gscip_constraint_handler_test.cc.
//
// Implementation details:
//
// * Default implementations: All callback functions have a default
// implementation that returns "did not run" or "feasible" accordingly. For
// rounding lock, the default implementation locks both directions.
//
// * Status errors: If the user returns an absl::Status error, then the solve is
// interrupted via SCIPinterruptSolve(), and the status error is ultimately
// returned by GScip::Solve() after SCIP completes the interruption. The
// callback function returns SCIP_OKAY to SCIP except for internal errors. We
// try to avoid returning SCIP_ERROR in the middle of a callback since SCIP
// might not stay in a fully clean state (e.g. calling SCIPfree might hit an
// assert).
//
// * Constraint priority: SCIP informs the callback which subset of constraints
// are more likely to be violated. The callback is called on those constraints
// first, and if the highest priority result is kDidNotFind, kDidNotRun, or
// kFeasible, it is called for the remaining ones.
//
// Supported SCIP callback functions:
//  * SCIP_DECL_CONSENFOLP
//  * SCIP_DECL_CONSENFOPS
//  * SCIP_DECL_CONSCHECK
//  * SCIP_DECL_CONSLOCK
//  * SCIP_DECL_CONSSEPALP
//  * SCIP_DECL_CONSSEPASOL
//
// Used, but not customizable:
//  * SCIP_DECL_CONSFREE
//  * SCIP_DECL_CONSINIT
//  * SCIP_DECL_CONSDELETE
template <typename ConstraintData>
class GScipConstraintHandler {
 public:
  // Constructs a constraint handler that will be registered using the given
  // properties. It is recommended to set priorities and frequencies manually in
  // properties.
  explicit GScipConstraintHandler(
      const GScipConstraintHandlerProperties& properties)
      : properties_(properties) {}

  virtual ~GScipConstraintHandler() = default;

  const GScipConstraintHandlerProperties& properties() const {
    return properties_;
  }

  // Registers this constraint handler with GScip. If the handler has already
  // been registered, returns an error.
  absl::Status Register(GScip* gscip);

  // Adds a callback constraint to the model. That is, it attaches to the
  // constraint handler a constraint for the given constraint data.
  absl::Status AddCallbackConstraint(GScip* gscip,
                                     std::string_view constraint_name,
                                     const ConstraintData* constraint_data,
                                     const GScipConstraintOptions& options);

  // Callback function called at SCIP's CONSENFOLP. Must check if an LP solution
  // at a node is feasible, and if not, resolve the infeasibility if possible by
  // branching, reducing variable domains, or separating the solution with a
  // cutting plane. If properties_.enforcement_priority < 0, then this only acts
  // on integer solutions.
  //
  // SCIP CONSENFOLP callback arguments:
  // * solution_infeasible: solinfeasible in SCIP, indicates if the solution was
  //   already declared infeasible by a constraint handler.
  //
  // It is the user's responsibility to return a valid result for CONSENFOLP;
  // see SCIP's documentation (e.g. type_cons.h).
  virtual absl::StatusOr<GScipCallbackResult> EnforceLp(
      GScipConstraintHandlerContext context,
      const ConstraintData& constraint_data, bool solution_infeasible);

  // Callback function called at SCIP's CONSENFOPS. Must check if a
  // pseudosolution is feasible, and if not, resolve the infeasibility if
  // possible by branching, reducing variable domains, or adding an additional
  // constraint. Separating with a cutting plane is not possible since there is
  // no corresponding LP (i.e. kSeparated cannot be returned). If
  // properties_.enforcement_priority < 0, then this only acts on integer
  // solutions.
  //
  // SCIP CONSENFOPS callback arguments:
  // * solution_infeasible: solinfeasible in SCIP, indicates if the solution was
  //   already declared infeasible by a constraint handler.
  // * objective_infeasible: objinfeasible in SCIP, indicates if the solution is
  //   infeasible due to violating objective bound.
  //
  // It is the user's responsibility to return a valid result for CONSENFOPS;
  // see SCIP's documentation (e.g. type_cons.h).
  virtual absl::StatusOr<GScipCallbackResult> EnforcePseudoSolution(
      GScipConstraintHandlerContext context,
      const ConstraintData& constraint_data, bool solution_infeasible,
      bool objective_infeasible);

  // Callback function called at SCIP's CONSCHECK. Must return true if the
  // current solution stored in the context satisfies all constraints of the
  // constraint handler, or false otherwise. If
  // properties_.feasibility_check_priority < 0, then this only acts on integer
  // solutions.
  //
  // SCIP CONSCHECK callback arguments:
  // * check_integrality: checkintegrality in SCIP, indicates if integrality
  //   must be checked. Used to avoid redundant checks in cases where
  //   integrality is already checked or implicit.
  // * check_lp_rows: checklprows in SCIP, indicates if the constraints
  //   represented by rows in the current LP must be checked. Used to avoid
  //   redundant checks in cases where row feasibility is already checked or
  //   implicit.
  // * print_reason: printreason in SCIP, indicates if the reason for the
  //   violation should be printed.
  // * check_completely: completely in SCIP, indicates if all violations should
  //   be checked.
  virtual absl::StatusOr<bool> CheckIsFeasible(
      GScipConstraintHandlerContext context,
      const ConstraintData& constraint_data, bool check_integrality,
      bool check_lp_rows, bool print_reason, bool check_completely);

  // Callback function called at SCIP's CONSLOCK. Must return, for each
  // variable, whether the constraint may be violated by decreasing and/or
  // increasing the variable value. It is always safe to claim that both
  // directions can violate the constraint, which is the default implementation,
  // but it may affect SCIP's capabilities.
  //
  // SCIP CONSLOCK callback arguments:
  // * lock_type_is_model: if locktype == SCIP_LOCKTYPE_MODEL in SCIP. If true,
  //   this callback is called for model constraints, otherwise it is called for
  //   conflict constraints.
  //
  // It is the user's responsibility to return a valid result for CONSLOCK; see
  // SCIP's documentation (e.g. type_cons.h).
  virtual std::vector<std::pair<SCIP_VAR*, RoundingLockDirection>> RoundingLock(
      GScip* gscip, const ConstraintData& constraint_data,
      bool lock_type_is_model);

  // Callback function called at SCIP's CONSSEPALP. Separates all constraints of
  // the constraint handler for LP solutions.
  //
  // It is the user's responsibility to return a valid result for CONSSEPALP;
  // see SCIP's documentation (e.g. type_cons.h).
  virtual absl::StatusOr<GScipCallbackResult> SeparateLp(
      GScipConstraintHandlerContext context,
      const ConstraintData& constraint_data);

  // Callback function called at SCIP's CONSSEPASOL. Separates all constraints
  // of the constraint handler for solutions that do not come from LP (e.g.
  // relaxators and primal heuristics).
  //
  // It is the user's responsibility to return a valid result for CONSSEPASOL;
  // see SCIP's documentation (e.g. type_cons.h).
  virtual absl::StatusOr<GScipCallbackResult> SeparateSolution(
      GScipConstraintHandlerContext context,
      const ConstraintData& constraint_data);

  // The functions below wrap each callback function to manage status.

  GScipCallbackResult CallEnforceLp(GScipConstraintHandlerContext context,
                                    const ConstraintData& constraint_data,
                                    bool solution_infeasible);

  GScipCallbackResult CallEnforcePseudoSolution(
      GScipConstraintHandlerContext context,
      const ConstraintData& constraint_data, bool solution_infeasible,
      bool objective_infeasible);

  GScipCallbackResult CallCheckIsFeasible(GScipConstraintHandlerContext context,
                                          const ConstraintData& constraint_data,
                                          bool check_integrality,
                                          bool check_lp_rows, bool print_reason,
                                          bool check_completely);

  GScipCallbackResult CallSeparateLp(GScipConstraintHandlerContext context,
                                     const ConstraintData& constraint_data);

  GScipCallbackResult CallSeparateSolution(
      GScipConstraintHandlerContext context,
      const ConstraintData& constraint_data);

 private:
  // If the result status is not OK, stores the status for GScip to later return
  // to the user, and interrupts the solve. Otherwise, returns the result
  // itself.
  GScipCallbackResult HandleCallbackStatus(
      absl::StatusOr<GScipCallbackResult> result,
      GScipConstraintHandlerContext context,
      GScipCallbackResult default_callback_result);

  GScipConstraintHandlerProperties properties_;
};

namespace internal {

// These classes implement a void pointer version of GScipConstraintHandler that
// performs a conversion to the more user-friendly templated implementation with
// ConstraintData. This parent class is needed as an untemplated version that
// SCIP_ConshdlrData can store.
class UntypedGScipConstraintHandler : public GScipConstraintHandler<void*> {
 public:
  explicit UntypedGScipConstraintHandler(
      const GScipConstraintHandlerProperties& properties)
      : GScipConstraintHandler<void*>(properties) {}
};

template <typename ConstraintData>
class UntypedGScipConstraintHandlerImpl : public UntypedGScipConstraintHandler {
 public:
  explicit UntypedGScipConstraintHandlerImpl(
      GScipConstraintHandler<ConstraintData>* constraint_handler)
      : UntypedGScipConstraintHandler(constraint_handler->properties()),
        actual_handler_(constraint_handler) {}

  absl::StatusOr<GScipCallbackResult> EnforceLp(
      GScipConstraintHandlerContext context, void* const& constraint_data,
      bool solution_infeasible) override {
    return actual_handler_->EnforceLp(
        context, *static_cast<const ConstraintData*>(constraint_data),
        solution_infeasible);
  }

  absl::StatusOr<GScipCallbackResult> EnforcePseudoSolution(
      GScipConstraintHandlerContext context, void* const& constraint_data,
      bool solution_infeasible, bool objective_infeasible) override {
    return actual_handler_->EnforcePseudoSolution(
        context, *static_cast<const ConstraintData*>(constraint_data),
        solution_infeasible, objective_infeasible);
  }

  absl::StatusOr<bool> CheckIsFeasible(GScipConstraintHandlerContext context,
                                       void* const& constraint_data,
                                       bool check_integrality,
                                       bool check_lp_rows, bool print_reason,
                                       bool completely) override {
    return actual_handler_->CheckIsFeasible(
        context, *static_cast<const ConstraintData*>(constraint_data),
        check_integrality, check_lp_rows, print_reason, completely);
  }

  std::vector<std::pair<SCIP_VAR*, RoundingLockDirection>> RoundingLock(
      GScip* gscip, void* const& constraint_data,
      bool lock_type_is_model) override {
    return actual_handler_->RoundingLock(
        gscip, *static_cast<const ConstraintData*>(constraint_data),
        lock_type_is_model);
  }

  absl::StatusOr<GScipCallbackResult> SeparateLp(
      GScipConstraintHandlerContext context,
      void* const& constraint_data) override {
    return actual_handler_->SeparateLp(
        context, *static_cast<const ConstraintData*>(constraint_data));
  }

  absl::StatusOr<GScipCallbackResult> SeparateSolution(
      GScipConstraintHandlerContext context,
      void* const& constraint_data) override {
    return actual_handler_->SeparateSolution(
        context, *static_cast<const ConstraintData*>(constraint_data));
  }

 private:
  GScipConstraintHandler<ConstraintData>* actual_handler_;
};

absl::Status RegisterConstraintHandler(
    GScip* gscip,
    std::unique_ptr<UntypedGScipConstraintHandler> constraint_handler);

absl::Status AddCallbackConstraint(GScip* gscip, std::string_view handler_name,
                                   std::string_view constraint_name,
                                   void* constraint_data,
                                   const GScipConstraintOptions& options);

}  // namespace internal

// Template implementations.

template <typename ConstraintData>
absl::Status GScipConstraintHandler<ConstraintData>::Register(GScip* gscip) {
  return internal::RegisterConstraintHandler(
      gscip,
      std::make_unique<
          internal::UntypedGScipConstraintHandlerImpl<ConstraintData>>(this));
}

template <typename ConstraintData>
absl::Status GScipConstraintHandler<ConstraintData>::AddCallbackConstraint(
    GScip* gscip, std::string_view constraint_name,
    const ConstraintData* constraint_data,
    const GScipConstraintOptions& options) {
  return internal::AddCallbackConstraint(
      gscip, properties().name, constraint_name,
      static_cast<void*>(const_cast<ConstraintData*>(constraint_data)),
      options);
}

// Default callback implementations.

template <typename ConstraintData>
absl::StatusOr<GScipCallbackResult>
GScipConstraintHandler<ConstraintData>::EnforceLp(
    GScipConstraintHandlerContext /*context*/,
    const ConstraintData& /*constraint_data*/, bool /*solution_infeasible*/) {
  return GScipCallbackResult::kFeasible;
}

template <typename ConstraintData>
absl::StatusOr<GScipCallbackResult>
GScipConstraintHandler<ConstraintData>::EnforcePseudoSolution(
    GScipConstraintHandlerContext /*context*/,
    const ConstraintData& /*constraint_data*/, bool /*solution_infeasible*/,
    bool /*objective_infeasible*/) {
  return GScipCallbackResult::kFeasible;
}

template <typename ConstraintData>
absl::StatusOr<bool> GScipConstraintHandler<ConstraintData>::CheckIsFeasible(
    GScipConstraintHandlerContext /*context*/,
    const ConstraintData& /*constraint_data*/, bool /*check_integrality*/,
    bool /*check_lp_rows*/, bool /*print_reason*/, bool /*check_completely*/) {
  return true;
}

template <typename ConstraintData>
std::vector<std::pair<SCIP_VAR*, RoundingLockDirection>>
GScipConstraintHandler<ConstraintData>::RoundingLock(
    GScip* gscip, const ConstraintData& /*constraint_data*/,
    bool /*lock_type_is_model*/) {
  std::vector<std::pair<SCIP_VAR*, RoundingLockDirection>> result;
  for (SCIP_VAR* var : gscip->variables()) {
    result.push_back({var, RoundingLockDirection::kBoth});
  }
  return result;
}

template <typename ConstraintData>
absl::StatusOr<GScipCallbackResult>
GScipConstraintHandler<ConstraintData>::SeparateLp(
    GScipConstraintHandlerContext /*context*/,
    const ConstraintData& /*constraint_data*/) {
  return GScipCallbackResult::kDidNotRun;
}

template <typename ConstraintData>
absl::StatusOr<GScipCallbackResult>
GScipConstraintHandler<ConstraintData>::SeparateSolution(
    GScipConstraintHandlerContext /*context*/,
    const ConstraintData& /*constraint_data*/) {
  return GScipCallbackResult::kDidNotRun;
}

// Internal functions to handle status.

template <typename ConstraintData>
GScipCallbackResult
GScipConstraintHandler<ConstraintData>::HandleCallbackStatus(
    const absl::StatusOr<GScipCallbackResult> result,
    GScipConstraintHandlerContext context,
    const GScipCallbackResult default_callback_result) {
  if (!result.ok()) {
    context.gscip()->InterruptSolveFromCallback(result.status());
    return default_callback_result;
  }
  return result.value();
}

template <typename ConstraintData>
GScipCallbackResult GScipConstraintHandler<ConstraintData>::CallEnforceLp(
    GScipConstraintHandlerContext context,
    const ConstraintData& constraint_data, bool solution_infeasible) {
  return HandleCallbackStatus(
      EnforceLp(context, constraint_data, solution_infeasible), context,
      GScipCallbackResult::kFeasible);
}

template <typename ConstraintData>
GScipCallbackResult
GScipConstraintHandler<ConstraintData>::CallEnforcePseudoSolution(
    GScipConstraintHandlerContext context,
    const ConstraintData& constraint_data, bool solution_infeasible,
    bool objective_infeasible) {
  return HandleCallbackStatus(
      EnforcePseudoSolution(context, constraint_data, solution_infeasible,
                            objective_infeasible),
      context, GScipCallbackResult::kFeasible);
}

template <typename ConstraintData>
GScipCallbackResult GScipConstraintHandler<ConstraintData>::CallCheckIsFeasible(
    GScipConstraintHandlerContext context,
    const ConstraintData& constraint_data, bool check_integrality,
    bool check_lp_rows, bool print_reason, bool check_completely) {
  const absl::StatusOr<bool> result =
      CheckIsFeasible(context, constraint_data, check_integrality,
                      check_lp_rows, print_reason, check_completely);
  if (!result.ok()) {
    return HandleCallbackStatus(result.status(), context,
                                GScipCallbackResult::kFeasible);
  } else {
    return HandleCallbackStatus(*result ? GScipCallbackResult::kFeasible
                                        : GScipCallbackResult::kInfeasible,
                                context, GScipCallbackResult::kFeasible);
  }
}

template <typename ConstraintData>
GScipCallbackResult GScipConstraintHandler<ConstraintData>::CallSeparateLp(
    GScipConstraintHandlerContext context,
    const ConstraintData& constraint_data) {
  return HandleCallbackStatus(SeparateLp(context, constraint_data), context,
                              GScipCallbackResult::kDidNotRun);
}

template <typename ConstraintData>
GScipCallbackResult
GScipConstraintHandler<ConstraintData>::CallSeparateSolution(
    GScipConstraintHandlerContext context,
    const ConstraintData& constraint_data) {
  return HandleCallbackStatus(SeparateSolution(context, constraint_data),
                              context, GScipCallbackResult::kDidNotRun);
}

}  // namespace operations_research

#endif  // OR_TOOLS_GSCIP_GSCIP_CONSTRAINT_HANDLER_H_
