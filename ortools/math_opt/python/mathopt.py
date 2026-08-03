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

"""Module exporting all classes and functions needed for MathOpt.

This module defines aliases to all classes and functions needed for regular use
of MathOpt. It removes the need for users to have multiple imports for specific
sub-modules.

For example instead of:
  from ortools.math_opt.python import model
  from ortools.math_opt.python import solve

  m = model.Model()
  solve.solve(m)

we can simply do:
  from ortools.math_opt.python import mathopt

  m = mathopt.Model()
  mathopt.solve(m)
"""

# pylint: disable=unused-import
# pylint: disable=g-importing-member

from ortools.math_opt.python.callback import (BarrierStats, CallbackData,
                                              CallbackRegistration,
                                              CallbackResult, Event,
                                              GeneratedConstraint, MipStats,
                                              PresolveStats, SimplexStats,
                                              parse_callback_data)
from ortools.math_opt.python.compute_infeasible_subsystem_result import (
    ComputeInfeasibleSubsystemResult, ModelSubset, ModelSubsetBounds,
    parse_compute_infeasible_subsystem_result, parse_model_subset,
    parse_model_subset_bounds)
from ortools.math_opt.python.errors import (InternalMathOptError,
                                            status_proto_to_exception)
from ortools.math_opt.python.expressions import evaluate_expression, fast_sum
from ortools.math_opt.python.indicator_constraints import IndicatorConstraint
from ortools.math_opt.python.init_arguments import (
    GurobiISVKey, StreamableCpSatInitArguments, StreamableEcosInitArguments,
    StreamableGlopInitArguments, StreamableGlpkInitArguments,
    StreamableGScipInitArguments, StreamableGurobiInitArguments,
    StreamableHighsInitArguments, StreamableOsqpInitArguments,
    StreamablePdlpInitArguments, StreamableSantoriniInitArguments,
    StreamableScsInitArguments, StreamableSolverInitArguments,
    gurobi_isv_key_from_proto, streamable_gurobi_init_arguments_from_proto,
    streamable_solver_init_arguments_from_proto)
from ortools.math_opt.python.linear_constraints import (
    LinearConstraint, LinearConstraintMatrixEntry)
from ortools.math_opt.python.message_callback import (SolveMessageCallback,
                                                      list_message_callback,
                                                      log_messages,
                                                      printer_message_callback,
                                                      vlog_messages)
from ortools.math_opt.python.model import Model, UpdateTracker
from ortools.math_opt.python.model_parameters import (
    ModelSolveParameters, ObjectiveParameters, SolutionHint,
    parse_objective_parameters, parse_solution_hint)
from ortools.math_opt.python.objectives import AuxiliaryObjective, Objective
from ortools.math_opt.python.parameters import (Emphasis, GlpkParameters,
                                                GurobiParameters, LPAlgorithm,
                                                SolveParameters, SolverType,
                                                emphasis_from_proto,
                                                emphasis_to_proto,
                                                lp_algorithm_from_proto,
                                                lp_algorithm_to_proto,
                                                parse_glpk_parameters,
                                                parse_gurobi_parameters,
                                                parse_solve_parameters,
                                                solver_type_from_proto,
                                                solver_type_to_proto)
from ortools.math_opt.python.quadratic_constraints import QuadraticConstraint
from ortools.math_opt.python.result import (FeasibilityStatus, Limit,
                                            ObjectiveBounds, ProblemStatus,
                                            SolveResult, SolveStats,
                                            Termination, TerminationReason,
                                            parse_objective_bounds,
                                            parse_problem_status,
                                            parse_solve_result,
                                            parse_solve_stats,
                                            parse_termination)
from ortools.math_opt.python.solution import (
    Basis, BasisStatus, DualRay, DualSolution, PrimalRay, PrimalSolution,
    Solution, SolutionStatus, optional_solution_status_to_proto, parse_basis,
    parse_dual_ray, parse_dual_solution, parse_optional_solution_status,
    parse_primal_ray, parse_primal_solution, parse_solution)
from ortools.math_opt.python.solve import (IncrementalSolver, SolveCallback,
                                           compute_infeasible_subsystem, solve)
from ortools.math_opt.python.solver_resources import SolverResources
from ortools.math_opt.python.sparse_containers import (
    LinearConstraintFilter, QuadraticConstraintFilter, SparseVectorFilter,
    VariableFilter, VarOrConstraintType, parse_linear_constraint_map,
    parse_quadratic_constraint_map, parse_variable_map,
    to_sparse_double_vector_proto, to_sparse_int32_vector_proto)
from ortools.math_opt.python.variables import (BoundedLinearExpression,
                                               BoundedLinearTypes,
                                               BoundedLinearTypesList,
                                               BoundedQuadraticExpression,
                                               BoundedQuadraticTypes,
                                               BoundedQuadraticTypesList,
                                               LinearBase, LinearExpression,
                                               LinearLinearProduct,
                                               LinearProduct, LinearSum,
                                               LinearTerm, LinearTypes,
                                               LinearTypesExceptVariable,
                                               LowerBoundedLinearExpression,
                                               LowerBoundedQuadraticExpression,
                                               QuadraticBase,
                                               QuadraticExpression,
                                               QuadraticProduct, QuadraticSum,
                                               QuadraticTerm, QuadraticTermKey,
                                               QuadraticTypes,
                                               UpperBoundedLinearExpression,
                                               UpperBoundedQuadraticExpression,
                                               VarEqVar, Variable,
                                               as_flat_linear_expression,
                                               as_flat_quadratic_expression)

# pylint: enable=unused-import
# pylint: enable=g-importing-member
