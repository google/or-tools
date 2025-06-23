#!/usr/bin/env python3
# Copyright 2010-2025 Google LLC
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

"""Solves a simple LP using PDLP's direct Python API.

Note: The direct API is generally for advanced use cases. It is matrix-based,
that is, you specify the LP using matrices and vectors instead of algebraic
expressions. You can also use PDLP via the algebraic pywraplp API (see
linear_solver/samples/simple_lp_program.py).
"""

import numpy as np
import scipy.sparse

from ortools.pdlp import solve_log_pb2
from ortools.pdlp import solvers_pb2
from ortools.pdlp.python import pdlp
from ortools.init.python import init


def simple_lp() -> pdlp.QuadraticProgram:
  """Returns a small LP.

  min 5.5 x_0 - 2 x_1 - x_2 +   x_3 - 14 s.t.
      2 x_0 +     x_1 +   x_2 + 2 x_3  = 12
        x_0 +             x_2          <=  7
      4 x_0                            >=  -4
     -1 <=            1.5 x_2 -   x_3  <= 1
    -infinity <= x_0 <= infinity
           -2 <= x_1 <= infinity
    -infinity <= x_2 <= 6
          2.5 <= x_3 <= 3.5
  """
  lp = pdlp.QuadraticProgram()
  lp.objective_offset = -14
  lp.objective_vector = [5.5, -2, -1, 1]
  lp.constraint_lower_bounds = [12, -np.inf, -4, -1]
  lp.constraint_upper_bounds = [12, 7, np.inf, 1]
  lp.variable_lower_bounds = [-np.inf, -2, -np.inf, 2.5]
  lp.variable_upper_bounds = [np.inf, np.inf, 6, 3.5]
  # Most use cases should initialize the sparse constraint matrix without
  # constructing a dense matrix first! We use a np.array here for convenience
  # only.
  constraint_matrix = np.array(
      [[2, 1, 1, 2], [1, 0, 1, 0], [4, 0, 0, 0], [0, 0, 1.5, -1]]
  )
  lp.constraint_matrix = scipy.sparse.csc_matrix(constraint_matrix)
  return lp


def main() -> None:
  params = solvers_pb2.PrimalDualHybridGradientParams()
  # Below are some common parameters to modify. Here, we just re-assign the
  # defaults.
  optimality_criteria = params.termination_criteria.simple_optimality_criteria
  optimality_criteria.eps_optimal_relative = 1.0e-6
  optimality_criteria.eps_optimal_absolute = 1.0e-6
  params.termination_criteria.time_sec_limit = np.inf
  params.num_threads = 1
  params.verbosity_level = 0
  params.presolve_options.use_glop = False

  # Call the main solve function.
  result = pdlp.primal_dual_hybrid_gradient(simple_lp(), params)
  solve_log = result.solve_log

  if solve_log.termination_reason == solve_log_pb2.TERMINATION_REASON_OPTIMAL:
    print("Solve successful")
  else:
    print(
        "Solve not successful. Status:",
        solve_log_pb2.TerminationReason.Name(solve_log.termination_reason),
    )

  # Solutions vectors are always returned. *However*, their interpretation
  # depends on termination_reason! See primal_dual_hybrid_gradient.h for more
  # details on what the vectors mean if termination_reason is not
  # TERMINATION_REASON_OPTIMAL.
  print("Primal solution:", result.primal_solution)
  print("Dual solution:", result.dual_solution)
  print("Reduced costs:", result.reduced_costs)

  solution_type = solve_log.solution_type
  print("Solution type:", solve_log_pb2.PointType.Name(solution_type))
  for ci in solve_log.solution_stats.convergence_information:
    if ci.candidate_type == solution_type:
      print("Primal objective:", ci.primal_objective)
      print("Dual objective:", ci.dual_objective)

  print("Iterations:", solve_log.iteration_count)
  print("Solve time (sec):", solve_log.solve_time_sec)


if __name__ == "__main__":
  init.CppBridge.init_logging("simple_pdlp_program.py")
  cpp_flags = init.CppFlags()
  cpp_flags.stderrthreshold = 0
  cpp_flags.log_prefix = False
  init.CppBridge.set_flags(cpp_flags)
  main()
