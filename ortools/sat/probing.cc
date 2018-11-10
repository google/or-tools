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

#include "ortools/sat/probing.h"

#include <set>

#include "ortools/base/timer.h"
#include "ortools/sat/clause.h"
#include "ortools/sat/integer.h"
#include "ortools/sat/sat_solver.h"

namespace operations_research {
namespace sat {

bool ProbeBooleanVariables(double deterministic_time_limit, Model* model) {
  WallTimer wall_timer;
  wall_timer.Start();

  // Reset the solver in case it was already used.
  auto* sat_solver = model->GetOrCreate<SatSolver>();
  sat_solver->SetAssumptionLevel(0);
  if (!sat_solver->RestoreSolverToAssumptionLevel()) return false;

  const int initial_num_fixed = sat_solver->LiteralTrail().Index();
  const double initial_deterministic_time = sat_solver->deterministic_time();
  const double limit = initial_deterministic_time + deterministic_time_limit;

  // For the new direct implication detected.
  std::set<std::pair<Literal, Literal>> new_binary_clauses;
  auto* implication_graph = model->GetOrCreate<BinaryImplicationGraph>();
  const int id = implication_graph->PropagatorId();

  // This is used to tighten the integer variable bounds.
  int num_new_holes = 0;
  int num_new_integer_bounds = 0;
  auto* integer_trail = model->GetOrCreate<IntegerTrail>();
  std::vector<IntegerLiteral> new_integer_bounds;

  bool limit_reached = false;
  const int num_variables = sat_solver->NumVariables();
  for (BooleanVariable b(0); b < num_variables; ++b) {
    if (sat_solver->deterministic_time() > limit) {
      limit_reached = true;
      break;
    }

    // Propagate b=1 and then b=0.
    new_integer_bounds.clear();
    for (const Literal decision : {Literal(b, true), Literal(b, false)}) {
      if (!sat_solver->RestoreSolverToAssumptionLevel()) return false;
      if (sat_solver->Assignment().LiteralIsAssigned(decision)) continue;

      const int saved_index = sat_solver->LiteralTrail().Index();
      sat_solver->EnqueueDecisionAndBackjumpOnConflict(decision);
      if (sat_solver->IsModelUnsat()) return false;
      integer_trail->AppendNewBounds(&new_integer_bounds);
      if (sat_solver->CurrentDecisionLevel() > 0) {
        for (int i = saved_index + 1; i < sat_solver->LiteralTrail().Index();
             ++i) {
          const Literal l = sat_solver->LiteralTrail()[i];
          if (sat_solver->LiteralTrail().AssignmentType(l.Variable()) == id) {
            continue;
          }
          new_binary_clauses.insert({decision.Negated(), l});
        }
      }
    }

    // We have at most two lower bounds for each variables (one for b==0 and one
    // for b==1), so the min of the two is a valid level zero bound! More
    // generally, the domain of a variable can be intersected with the union
    // of the two propagated domains. This also allow to detect "holes".
    //
    // TODO(user): fix binary variable in the same way? It might not be as
    // useful since probing on such variable will also fix it. But then we might
    // abort probing early, so it might still be good.
    sat_solver->Backtrack(0);
    std::sort(new_integer_bounds.begin(), new_integer_bounds.end(),
              [](IntegerLiteral a, IntegerLiteral b) { return a.var < b.var; });

    // This is used for the hole detection.
    IntegerVariable prev_var = kNoIntegerVariable;
    IntegerValue lb_max = kMinIntegerValue;
    IntegerValue ub_min = kMaxIntegerValue;
    new_integer_bounds.push_back(IntegerLiteral());  // Sentinel.

    for (int i = 1; i < new_integer_bounds.size(); ++i) {
      const IntegerVariable var = new_integer_bounds[i].var;

      // Hole detection.
      if (PositiveVariable(var) != prev_var) {
        if (ub_min + 1 < lb_max) {
          // The variable cannot take value in (ub_min, lb_max) !
          //
          // TODO(user): do not create domain with a complexity that is too
          // large?
          const Domain old_domain =
              integer_trail->InitialVariableDomain(prev_var);
          const Domain new_domain = old_domain.IntersectionWith(
              Domain(ub_min.value() + 1, lb_max.value() - 1).Complement());
          if (new_domain != old_domain) {
            ++num_new_holes;
            if (!integer_trail->UpdateInitialDomain(prev_var, new_domain)) {
              return false;
            }
          }
        }

        // Reinitialize.
        prev_var = PositiveVariable(var);
        lb_max = kMinIntegerValue;
        ub_min = kMaxIntegerValue;
      }
      if (VariableIsPositive(var)) {
        lb_max = std::max(lb_max, new_integer_bounds[i].bound);
      } else {
        ub_min = std::min(ub_min, -new_integer_bounds[i].bound);
      }

      // Bound tightening.
      if (new_integer_bounds[i - 1].var != var) continue;
      const IntegerValue new_bound = std::min(new_integer_bounds[i - 1].bound,
                                              new_integer_bounds[i].bound);
      if (new_bound > integer_trail->LowerBound(var)) {
        ++num_new_integer_bounds;
        if (!integer_trail->Enqueue(
                IntegerLiteral::GreaterOrEqual(var, new_bound), {}, {})) {
          return false;
        }
      }
    }
  }

  // Adds the newly discovered direct implication for a faster propagation
  // and so that they can be used for binary equivalence detection.
  //
  // TODO(user): do that on the fly for a faster probing? also use hash_map.
  sat_solver->Backtrack(0);
  for (const auto binary : new_binary_clauses) {
    sat_solver->AddBinaryClause(binary.first, binary.second);
  }

  // Display stats.
  const double time_diff =
      sat_solver->deterministic_time() - initial_deterministic_time;
  const int num_fixed = sat_solver->LiteralTrail().Index();
  const int num_newly_fixed = num_fixed - initial_num_fixed;
  VLOG(1) << "Probing deterministic_time: " << time_diff
          << " wall_time: " << wall_timer.Get()
          << (limit_reached ? " (Aborted)" : "");
  VLOG_IF(1, num_newly_fixed > 0)
      << "Probing new fixed binary: " << num_newly_fixed << " (" << num_fixed
      << "/" << num_variables << " overall)";
  VLOG_IF(1, num_new_holes > 0)
      << "Probing new integer holes: " << num_new_holes;
  VLOG_IF(1, num_new_integer_bounds > 0)
      << "Probing new integer bounds: " << num_new_integer_bounds;
  VLOG_IF(1, !new_binary_clauses.empty())
      << "Probing new binary clause: " << new_binary_clauses.size();

  return true;
}

}  // namespace sat
}  // namespace operations_research
