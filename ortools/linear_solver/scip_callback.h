// Copyright 2010-2018 Google LLC
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

// See go/scip-callbacks for documentation.
//
// This file provides a simplified C++ API for using callbacks with SCIP and
// MPSolver.  It can be used directly by users, although in most cases, the
// mp_callback.h should be sufficient (in fact, SCIP's mp_callback.h
// implementation is built on top of this).  See also go/mpsolver-callbacks.

#ifndef OR_TOOLS_LINEAR_SOLVER_SCIP_CALLBACK_H_
#define OR_TOOLS_LINEAR_SOLVER_SCIP_CALLBACK_H_

#include <string>
#include <vector>

#include "absl/memory/memory.h"
#include "ortools/linear_solver/linear_expr.h"
#include "ortools/linear_solver/linear_solver.h"
#include "scip/scip_sol.h"
#include "scip/type_cons.h"
#include "scip/type_scip.h"
#include "scip/type_sol.h"
#include "scip/type_var.h"

namespace operations_research {

// See https://scip.zib.de/doc-6.0.2/html/CONS.php#CONS_PROPERTIES for details.
// For member below, the corresponding SCIP constraint handler property name is
// provided.
//
// TODO(user): no effort has been made to optimize the default values of
// enforcement_priority, feasibility_check_priority, eager_frequency, or
// separation_priority.
struct ScipConstraintHandlerDescription {
  // See CONSHDLR_NAME in SCIP documentation above.
  std::string name;

  // See CONSHDLR_DESC in SCIP documentation above.
  std::string description;

  // See CONSHDLR_ENFOPRIORITY in the SCIP documentation above. Determines the
  // order this constraint class is checked at each LP node.
  //
  // WARNING(rander): Assumed that enforcement_priority < 0. (This enforcement
  // runs after integrality enforcement, so CONSENFOLP always runs on integral
  // solutions.)
  int enforcement_priority = -100;

  // See CONSHDLR_CHECKPRIORITY: in the SCIP documentation above. Determines the
  // order this constraint class runs in when testing solution feasibility.
  //
  // WARNING(rander): Assumed that feasibility_check_priority < 0. (This check
  // runs after the integrality check, so CONSCHECK always runs on integral
  // solutions.)
  int feasibility_check_priority = -100;

  // See CONSHDLR_EAGERFREQ in SCIP documentation above.
  int eager_frequency = 10;

  // See CONSHDLR_NEEDSCONS in SCIP documentation above.
  bool needs_constraints = false;

  // See CONSHDLR_SEPAPRIORITY in SCIP documentation above. Determines the
  // order this constraint class runs in the cut loop.
  int separation_priority = 100;

  // See CONSHDLR_SEPAFREQ in the SCIP documentation above.
  int separation_frequency = 1;
};

class ScipConstraintHandlerContext {
 public:
  // A value of nullptr for solution means to use the current LP solution.
  ScipConstraintHandlerContext(SCIP* scip, SCIP_SOL* solution,
                               bool is_pseudo_solution);
  double VariableValue(const MPVariable* variable) const;

  int64 CurrentNodeId() const;
  int64 NumNodesProcessed() const;

  SCIP* scip() const { return scip_; }

  // Pseudo solutions may not be LP feasible.  Duals/reduced costs are not
  // available (the LP solver failed at this node).
  //
  // Do not add "user cuts" here (that strengthen LP solution but don't change
  // feasible region), add only "lazy constraints" (cut off integer solutions).
  //
  // TODO(user): maybe this can be abstracted away.
  bool is_pseudo_solution() const { return is_pseudo_solution_; }

 private:
  SCIP* scip_;
  SCIP_SOL* solution_;
  bool is_pseudo_solution_;
};

struct CallbackRangeConstraint {
  LinearRange range;
  bool is_cut = false;  // Does not remove any integer points.
  std::string name;     // can be empty
  bool local = false;
};

// See go/scip-callbacks for additional documentation.
template <typename Constraint>
class ScipConstraintHandler {
 public:
  explicit ScipConstraintHandler(
      const ScipConstraintHandlerDescription& description)
      : description_(description) {}
  virtual ~ScipConstraintHandler() {}
  const ScipConstraintHandlerDescription& description() const {
    return description_;
  }

  // Unless SeparateIntegerSolution() below is overridden, this must find a
  // violated lazy constraint if one exists when given an integral solution.
  virtual std::vector<CallbackRangeConstraint> SeparateFractionalSolution(
      const ScipConstraintHandlerContext& context,
      const Constraint& constraint) = 0;

  // This MUST find a violated lazy constraint if one exists.
  // All constraints returned must have is_cut as false.
  virtual std::vector<CallbackRangeConstraint> SeparateIntegerSolution(
      const ScipConstraintHandlerContext& context,
      const Constraint& constraint) {
    return SeparateFractionalSolution(context, constraint);
  }

  // Returns true if no constraints are violated.
  virtual bool FractionalSolutionFeasible(
      const ScipConstraintHandlerContext& context,
      const Constraint& constraint) {
    return SeparateFractionalSolution(context, constraint).empty();
  }

  // This MUST find a violated constraint if one exists.
  virtual bool IntegerSolutionFeasible(
      const ScipConstraintHandlerContext& context,
      const Constraint& constraint) {
    return SeparateIntegerSolution(context, constraint).empty();
  }

 private:
  ScipConstraintHandlerDescription description_;
};

// handler is not owned but held.
template <typename Constraint>
void RegisterConstraintHandler(ScipConstraintHandler<Constraint>* handler,
                               SCIP* scip);

struct ScipCallbackConstraintOptions {
  bool initial = true;
  bool separate = true;
  bool enforce = true;
  bool check = true;
  bool propagate = true;
  bool local = false;
  bool modifiable = false;
  bool dynamic = false;
  bool removable = true;
  bool stickingatnodes = false;
};

// constraint_data is not owned but held.
template <typename ConstraintData>
void AddCallbackConstraint(SCIP* scip,
                           ScipConstraintHandler<ConstraintData>* handler,
                           const std::string& constraint_name,
                           const ConstraintData* constraint_data,
                           const ScipCallbackConstraintOptions& options);

// Implementation details, here and below.

namespace internal {

class ScipCallbackRunner {
 public:
  virtual ~ScipCallbackRunner() {}
  virtual std::vector<CallbackRangeConstraint> SeparateFractionalSolution(
      const ScipConstraintHandlerContext& context, void* constraint) = 0;

  virtual std::vector<CallbackRangeConstraint> SeparateIntegerSolution(
      const ScipConstraintHandlerContext& context, void* constraint) = 0;

  virtual bool FractionalSolutionFeasible(
      const ScipConstraintHandlerContext& context, void* constraint) = 0;

  virtual bool IntegerSolutionFeasible(
      const ScipConstraintHandlerContext& context, void* constraint) = 0;
};

template <typename ConstraintData>
class ScipCallbackRunnerImpl : public ScipCallbackRunner {
 public:
  explicit ScipCallbackRunnerImpl(
      ScipConstraintHandler<ConstraintData>* handler)
      : handler_(handler) {}

  std::vector<CallbackRangeConstraint> SeparateFractionalSolution(
      const ScipConstraintHandlerContext& context,
      void* constraint_data) override {
    return handler_->SeparateFractionalSolution(
        context, *static_cast<ConstraintData*>(constraint_data));
  }

  std::vector<CallbackRangeConstraint> SeparateIntegerSolution(
      const ScipConstraintHandlerContext& context,
      void* constraint_data) override {
    return handler_->SeparateIntegerSolution(
        context, *static_cast<ConstraintData*>(constraint_data));
  }

  bool FractionalSolutionFeasible(const ScipConstraintHandlerContext& context,
                                  void* constraint_data) override {
    return handler_->FractionalSolutionFeasible(
        context, *static_cast<ConstraintData*>(constraint_data));
  }

  bool IntegerSolutionFeasible(const ScipConstraintHandlerContext& context,
                               void* constraint_data) override {
    return handler_->IntegerSolutionFeasible(
        context, *static_cast<ConstraintData*>(constraint_data));
  }

 private:
  ScipConstraintHandler<ConstraintData>* handler_;
};

void AddConstraintHandlerImpl(
    const ScipConstraintHandlerDescription& description,
    std::unique_ptr<ScipCallbackRunner> runner, SCIP* scip);

void AddCallbackConstraintImpl(SCIP* scip, const std::string& handler_name,
                               const std::string& constraint_name,
                               void* constraint_data,
                               const ScipCallbackConstraintOptions& options);

}  // namespace internal

template <typename ConstraintData>
void RegisterConstraintHandler(ScipConstraintHandler<ConstraintData>* handler,
                               SCIP* scip) {
  internal::AddConstraintHandlerImpl(
      handler->description(),
      absl::make_unique<internal::ScipCallbackRunnerImpl<ConstraintData>>(
          handler),
      scip);
}

template <typename ConstraintData>
void AddCallbackConstraint(SCIP* scip,
                           ScipConstraintHandler<ConstraintData>* handler,
                           const std::string& constraint_name,
                           const ConstraintData* constraint_data,
                           const ScipCallbackConstraintOptions& options) {
  internal::AddCallbackConstraintImpl(
      scip, handler->description().name, constraint_name,
      static_cast<void*>(const_cast<ConstraintData*>(constraint_data)),
      options);
}

}  // namespace operations_research

#endif  // OR_TOOLS_LINEAR_SOLVER_SCIP_CALLBACK_H_
