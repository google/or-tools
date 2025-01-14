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

"""Model specific solver configuration (e.g. starting basis)."""
import dataclasses
from typing import Dict, List, Optional

from ortools.math_opt import model_parameters_pb2
from ortools.math_opt.python import model
from ortools.math_opt.python import solution
from ortools.math_opt.python import sparse_containers


@dataclasses.dataclass
class SolutionHint:
    """A suggested starting solution for the solver.

    MIP solvers generally only want primal information (`variable_values`),
    while LP solvers want both primal and dual information (`dual_values`).

    Many MIP solvers can work with: (1) partial solutions that do not specify all
    variables or (2) infeasible solutions. In these cases, solvers typically solve
    a sub-MIP to complete/correct the hint.

    How the hint is used by the solver, if at all, is highly dependent on the
    solver, the problem type, and the algorithm used. The most reliable way to
    ensure your hint has an effect is to read the underlying solvers logs with
    and without the hint.

    Simplex-based LP solvers typically prefer an initial basis to a solution
    hint (they need to crossover to convert the hint to a basic feasible
    solution otherwise).

    Floating point values should be finite and not NaN, they are validated by
    MathOpt at Solve() time (resulting in an exception).

    Attributes:
      variable_values: a potentially partial assignment from the model's primal
        variables to finite (and not NaN) double values.
      dual_values: a potentially partial assignment from the model's linear
        constraints to finite (and not NaN) double values.
    """

    variable_values: Dict[model.Variable, float] = dataclasses.field(
        default_factory=dict
    )
    dual_values: Dict[model.LinearConstraint, float] = dataclasses.field(
        default_factory=dict
    )

    def to_proto(self) -> model_parameters_pb2.SolutionHintProto:
        """Returns an equivalent protocol buffer to this."""
        return model_parameters_pb2.SolutionHintProto(
            variable_values=sparse_containers.to_sparse_double_vector_proto(
                self.variable_values
            ),
            dual_values=sparse_containers.to_sparse_double_vector_proto(
                self.dual_values
            ),
        )


def parse_solution_hint(
    hint_proto: model_parameters_pb2.SolutionHintProto, mod: model.Model
) -> SolutionHint:
    """Returns an equivalent SolutionHint to `hint_proto`.

    Args:
      hint_proto: The solution, as encoded by the ids of the variables and
        constraints.
      mod: A MathOpt Model that must contain variables and linear constraints with
        the ids from hint_proto.

    Returns:
      A SolutionHint equivalent.

    Raises:
      ValueError if hint_proto is invalid or refers to variables or constraints
      not in mod.
    """
    return SolutionHint(
        variable_values=sparse_containers.parse_variable_map(
            hint_proto.variable_values, mod
        ),
        dual_values=sparse_containers.parse_linear_constraint_map(
            hint_proto.dual_values, mod
        ),
    )


@dataclasses.dataclass
class ModelSolveParameters:
    """Model specific solver configuration, for example, an initial basis.

    This class mirrors (and can generate) the related proto
    model_parameters_pb2.ModelSolveParametersProto.

    Attributes:
      variable_values_filter: Only return solution and primal ray values for
        variables accepted by this filter (default accepts all variables).
      dual_values_filter: Only return dual variable values and dual ray values for
        linear constraints accepted by thei filter (default accepts all linear
        constraints).
      reduced_costs_filter: Only return reduced cost and dual ray values for
        variables accepted by this filter (default accepts all variables).
      initial_basis: If set, provides a warm start for simplex based solvers.
      solution_hints: Optional solution hints. If the underlying solver only
        accepts a single hint, the first hint is used.
      branching_priorities: Optional branching priorities. Variables with higher
        values will be branched on first. Variables for which priorities are not
        set get the solver's default priority (usually zero).
    """

    variable_values_filter: sparse_containers.VariableFilter = (
        sparse_containers.VariableFilter()
    )
    dual_values_filter: sparse_containers.LinearConstraintFilter = (
        sparse_containers.LinearConstraintFilter()
    )
    reduced_costs_filter: sparse_containers.VariableFilter = (
        sparse_containers.VariableFilter()
    )
    initial_basis: Optional[solution.Basis] = None
    solution_hints: List[SolutionHint] = dataclasses.field(default_factory=list)
    branching_priorities: Dict[model.Variable, int] = dataclasses.field(
        default_factory=dict
    )

    def to_proto(self) -> model_parameters_pb2.ModelSolveParametersProto:
        """Returns an equivalent protocol buffer."""
        # TODO(b/236289022): these methods should check that the variables are from
        # the correct model.
        result = model_parameters_pb2.ModelSolveParametersProto(
            variable_values_filter=self.variable_values_filter.to_proto(),
            dual_values_filter=self.dual_values_filter.to_proto(),
            reduced_costs_filter=self.reduced_costs_filter.to_proto(),
            branching_priorities=sparse_containers.to_sparse_int32_vector_proto(
                self.branching_priorities
            ),
        )
        if self.initial_basis:
            result.initial_basis.CopyFrom(self.initial_basis.to_proto())
        for hint in self.solution_hints:
            result.solution_hints.append(hint.to_proto())
        return result
