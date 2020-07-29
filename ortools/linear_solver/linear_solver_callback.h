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

// See go/mpsolver-callbacks for documentation on how to use this file.

#ifndef OR_TOOLS_LINEAR_SOLVER_LINEAR_SOLVER_CALLBACK_H_
#define OR_TOOLS_LINEAR_SOLVER_LINEAR_SOLVER_CALLBACK_H_

#include <string>

#include "absl/container/flat_hash_map.h"
#include "ortools/base/integral_types.h"

namespace operations_research {

class MPVariable;
class LinearExpr;
class LinearRange;

// The current state of the solver when the callback is invoked.
//
// For Gurobi, similar to the int 'where' in the Gurobi callback API.
// See http://www.gurobi.com/documentation/8.0/refman/callback_codes.html
// for details.
enum class MPCallbackEvent {
  kUnknown,
  // For regaining control of the main thread in single threaded applications,
  // not for interacting with the solver.
  kPolling,
  // The solver is currently running presolve.
  kPresolve,
  // The solver is currently running the simplex method.
  kSimplex,
  // The solver is in the MIP loop (called periodically before starting a new
  // node).  Useful to early termination.
  kMip,
  // Called every time a new MIP incumbent is found.
  kMipSolution,
  // Called once per pass of the cut loop inside each MIP node.
  kMipNode,
  // Called in each iterate of IPM/barrier method.
  kBarrier,
  // The solver is about to log out a message, use this callback to capture it.
  kMessage,
  // The solver is in multi-objective optimization.
  kMultiObj,
};

std::string ToString(MPCallbackEvent event);

// When querying solution values or modifying the model during a callback, use
// this API, rather than manipulating MPSolver directly.  You should only
// interact with this object from within MPCallback::RunCallback().
class MPCallbackContext {
 public:
  virtual ~MPCallbackContext() {}

  // What the solver is currently doing.  How you can interact with the solver
  // from the callback depends on this value.
  virtual MPCallbackEvent Event() = 0;

  // Always false if event is not kMipSolution or kMipNode, otherwise behavior
  // may be solver dependent.
  //
  // For Gurobi, under kMipNode, may be false if the node was not solved to
  // optimality, see MIPNODE_REL here for details:
  // http://www.gurobi.com/documentation/8.0/refman/callback_codes.html
  virtual bool CanQueryVariableValues() = 0;

  // Returns the value of variable from the solver's current state.
  //
  // Call only when CanQueryVariableValues() is true.
  //
  // At kMipSolution, the solution is integer feasible, while at kMipNode, the
  // solution solves the current node's LP relaxation (so integer variables may
  // be fractional).
  virtual double VariableValue(const MPVariable* variable) = 0;

  // Adds a constraint to the model that strengths the LP relaxation.
  //
  // Call only when the event is kMipNode.
  //
  // Requires that MPCallback::might_add_cuts() is true.
  //
  // This constraint must not cut off integer solutions, it should only
  // strengthen the LP (behavior is undefined otherwise).  Use
  // MPCallbackContext::AddLazyConstriant() if you are cutting off integer
  // solutions.
  virtual void AddCut(const LinearRange& cutting_plane) = 0;

  // Adds a constraint to the model that cuts off an undesired integer solution.
  //
  // Call only when the event is kMipSolution or kMipNode.
  //
  // Requires that MPCallback::might_add_lazy_constraints() is true.
  //
  // Use this to avoid adding a large number of constraints to the model where
  // most are expected to not be needed.
  //
  // Given an integral solution, AddLazyConstraint() MUST be able to detect if
  // there is a violated constraint, and it is guaranteed that every integer
  // solution will be checked by AddLazyConstraint().
  //
  // Warning(rander): in some solvers, e.g. Gurobi, an integer solution may not
  // respect a previously added lazy constraint, so you may need to add a
  // constraint more than once (e.g. due to threading issues).
  virtual void AddLazyConstraint(const LinearRange& lazy_constraint) = 0;

  // Suggests a (potentially partial) variable assignment to the solver, to be
  // used as a feasible solution (or part of one). If the assignment is partial,
  // certain solvers (e.g. Gurobi) will try to compute a feasible solution from
  // the partial assignment. Returns the objective value of the solution if the
  // solver supports it.
  //
  // Call only when the event is kMipNode.
  virtual double SuggestSolution(
      const absl::flat_hash_map<const MPVariable*, double>& solution) = 0;

  // Returns the number of nodes explored so far in the branch and bound tree,
  // which 0 at the root node and > 0 otherwise.
  //
  // Call only when the event is kMipSolution or kMipNode.
  virtual int64 NumExploredNodes() = 0;
};

// Extend this class with model specific logic, and register through
// MPSolver::SetCallback, passing a pointer to this object.
//
// See go/mpsolver-callbacks for additional documentation.
class MPCallback {
 public:
  // If you intend to call call MPCallbackContext::AddCut(), you must set
  // might_add_cuts below to be true.  Likewise for
  // MPCallbackContext::AddLazyConstraint() and might_add_lazy_constraints.
  MPCallback(bool might_add_cuts, bool might_add_lazy_constraints)
      : might_add_cuts_(might_add_cuts),
        might_add_lazy_constraints_(might_add_lazy_constraints) {}
  virtual ~MPCallback() {}

  // Threading behavior may be solver dependent:
  //   * Gurobi: RunCallback always runs on the same thread that you called
  //     MPSolver::Solve() on, even when Gurobi uses multiple threads.
  virtual void RunCallback(MPCallbackContext* callback_context) = 0;

  bool might_add_cuts() const { return might_add_cuts_; }
  bool might_add_lazy_constraints() const {
    return might_add_lazy_constraints_;
  }

 private:
  bool might_add_cuts_;
  bool might_add_lazy_constraints_;
};

// Single callback that runs the list of callbacks given at construction, in
// sequence.
class MPCallbackList : public MPCallback {
 public:
  explicit MPCallbackList(const std::vector<MPCallback*>& callbacks);

  // Runs all callbacks from the list given at construction, in sequence.
  void RunCallback(MPCallbackContext* context) override;

 private:
  const std::vector<MPCallback*> callbacks_;
};

}  // namespace operations_research

#endif  // OR_TOOLS_LINEAR_SOLVER_LINEAR_SOLVER_CALLBACK_H_
