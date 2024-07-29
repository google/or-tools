# Copyright 2010-2024 Google LLC
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

from ortools.math_opt.python.callback import BarrierStats
from ortools.math_opt.python.callback import CallbackData
from ortools.math_opt.python.callback import CallbackRegistration
from ortools.math_opt.python.callback import CallbackResult
from ortools.math_opt.python.callback import Event
from ortools.math_opt.python.callback import GeneratedConstraint
from ortools.math_opt.python.callback import MipStats
from ortools.math_opt.python.callback import parse_callback_data
from ortools.math_opt.python.callback import PresolveStats
from ortools.math_opt.python.callback import SimplexStats
from ortools.math_opt.python.compute_infeasible_subsystem_result import (
    ComputeInfeasibleSubsystemResult,
)
from ortools.math_opt.python.compute_infeasible_subsystem_result import ModelSubset
from ortools.math_opt.python.compute_infeasible_subsystem_result import (
    ModelSubsetBounds,
)
from ortools.math_opt.python.compute_infeasible_subsystem_result import (
    parse_compute_infeasible_subsystem_result,
)
from ortools.math_opt.python.compute_infeasible_subsystem_result import (
    parse_model_subset,
)
from ortools.math_opt.python.compute_infeasible_subsystem_result import (
    parse_model_subset_bounds,
)
from ortools.math_opt.python.errors import InternalMathOptError
from ortools.math_opt.python.errors import status_proto_to_exception
from ortools.math_opt.python.expressions import evaluate_expression
from ortools.math_opt.python.expressions import fast_sum
from ortools.math_opt.python.hash_model_storage import HashModelStorage
from ortools.math_opt.python.message_callback import list_message_callback
from ortools.math_opt.python.message_callback import log_messages
from ortools.math_opt.python.message_callback import printer_message_callback
from ortools.math_opt.python.message_callback import SolveMessageCallback
from ortools.math_opt.python.message_callback import vlog_messages
from ortools.math_opt.python.model import as_flat_linear_expression
from ortools.math_opt.python.model import as_flat_quadratic_expression
from ortools.math_opt.python.model import as_normalized_linear_inequality
from ortools.math_opt.python.model import BoundedLinearExpression
from ortools.math_opt.python.model import BoundedLinearTypes
from ortools.math_opt.python.model import BoundedLinearTypesList
from ortools.math_opt.python.model import LinearBase
from ortools.math_opt.python.model import LinearConstraint
from ortools.math_opt.python.model import LinearConstraintMatrixEntry
from ortools.math_opt.python.model import LinearExpression
from ortools.math_opt.python.model import LinearLinearProduct
from ortools.math_opt.python.model import LinearProduct
from ortools.math_opt.python.model import LinearSum
from ortools.math_opt.python.model import LinearTerm
from ortools.math_opt.python.model import LinearTypes
from ortools.math_opt.python.model import LinearTypesExceptVariable
from ortools.math_opt.python.model import LowerBoundedLinearExpression
from ortools.math_opt.python.model import Model
from ortools.math_opt.python.model import NormalizedLinearInequality
from ortools.math_opt.python.model import Objective
from ortools.math_opt.python.model import QuadraticBase
from ortools.math_opt.python.model import QuadraticExpression
from ortools.math_opt.python.model import QuadraticProduct
from ortools.math_opt.python.model import QuadraticSum
from ortools.math_opt.python.model import QuadraticTerm
from ortools.math_opt.python.model import QuadraticTermKey
from ortools.math_opt.python.model import QuadraticTypes
from ortools.math_opt.python.model import Storage
from ortools.math_opt.python.model import StorageClass
from ortools.math_opt.python.model import UpdateTracker
from ortools.math_opt.python.model import UpperBoundedLinearExpression
from ortools.math_opt.python.model import VarEqVar
from ortools.math_opt.python.model import Variable
from ortools.math_opt.python.model_parameters import ModelSolveParameters
from ortools.math_opt.python.model_parameters import parse_solution_hint
from ortools.math_opt.python.model_parameters import SolutionHint
from ortools.math_opt.python.model_storage import BadLinearConstraintIdError
from ortools.math_opt.python.model_storage import BadVariableIdError
from ortools.math_opt.python.model_storage import LinearConstraintMatrixIdEntry
from ortools.math_opt.python.model_storage import LinearObjectiveEntry
from ortools.math_opt.python.model_storage import ModelStorage
from ortools.math_opt.python.model_storage import ModelStorageImpl
from ortools.math_opt.python.model_storage import ModelStorageImplClass
from ortools.math_opt.python.model_storage import QuadraticEntry
from ortools.math_opt.python.model_storage import QuadraticTermIdKey
from ortools.math_opt.python.model_storage import StorageUpdateTracker
from ortools.math_opt.python.model_storage import UsedUpdateTrackerAfterRemovalError
from ortools.math_opt.python.parameters import Emphasis
from ortools.math_opt.python.parameters import emphasis_from_proto
from ortools.math_opt.python.parameters import emphasis_to_proto
from ortools.math_opt.python.parameters import GlpkParameters
from ortools.math_opt.python.parameters import GurobiParameters
from ortools.math_opt.python.parameters import lp_algorithm_from_proto
from ortools.math_opt.python.parameters import lp_algorithm_to_proto
from ortools.math_opt.python.parameters import LPAlgorithm
from ortools.math_opt.python.parameters import SolveParameters
from ortools.math_opt.python.parameters import solver_type_from_proto
from ortools.math_opt.python.parameters import solver_type_to_proto
from ortools.math_opt.python.parameters import SolverType
from ortools.math_opt.python.result import FeasibilityStatus
from ortools.math_opt.python.result import Limit
from ortools.math_opt.python.result import ObjectiveBounds
from ortools.math_opt.python.result import parse_objective_bounds
from ortools.math_opt.python.result import parse_problem_status
from ortools.math_opt.python.result import parse_solve_result
from ortools.math_opt.python.result import parse_solve_stats
from ortools.math_opt.python.result import parse_termination
from ortools.math_opt.python.result import ProblemStatus
from ortools.math_opt.python.result import SolveResult
from ortools.math_opt.python.result import SolveStats
from ortools.math_opt.python.result import Termination
from ortools.math_opt.python.result import TerminationReason
from ortools.math_opt.python.solution import Basis
from ortools.math_opt.python.solution import BasisStatus
from ortools.math_opt.python.solution import DualRay
from ortools.math_opt.python.solution import DualSolution
from ortools.math_opt.python.solution import optional_solution_status_to_proto
from ortools.math_opt.python.solution import parse_basis
from ortools.math_opt.python.solution import parse_dual_ray
from ortools.math_opt.python.solution import parse_dual_solution
from ortools.math_opt.python.solution import parse_optional_solution_status
from ortools.math_opt.python.solution import parse_primal_ray
from ortools.math_opt.python.solution import parse_primal_solution
from ortools.math_opt.python.solution import parse_solution
from ortools.math_opt.python.solution import PrimalRay
from ortools.math_opt.python.solution import PrimalSolution
from ortools.math_opt.python.solution import Solution
from ortools.math_opt.python.solution import SolutionStatus
from ortools.math_opt.python.solve import compute_infeasible_subsystem
from ortools.math_opt.python.solve import IncrementalSolver
from ortools.math_opt.python.solve import solve
from ortools.math_opt.python.solve import SolveCallback
from ortools.math_opt.python.solver_resources import SolverResources
from ortools.math_opt.python.sparse_containers import LinearConstraintFilter
from ortools.math_opt.python.sparse_containers import parse_linear_constraint_map
from ortools.math_opt.python.sparse_containers import parse_variable_map
from ortools.math_opt.python.sparse_containers import SparseVectorFilter
from ortools.math_opt.python.sparse_containers import to_sparse_double_vector_proto
from ortools.math_opt.python.sparse_containers import to_sparse_int32_vector_proto
from ortools.math_opt.python.sparse_containers import VariableFilter
from ortools.math_opt.python.sparse_containers import VarOrConstraintType

# pylint: enable=unused-import
# pylint: enable=g-importing-member
