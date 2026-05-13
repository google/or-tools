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
import datetime
from typing import Dict, List, Optional, Set

from ortools.math_opt import model_parameters_pb2
from ortools.math_opt.python import linear_constraints
from ortools.math_opt.python import model
from ortools.math_opt.python import objectives
from ortools.math_opt.python import solution
from ortools.math_opt.python import sparse_containers
from ortools.math_opt.python import variables


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

    variable_values: Dict[variables.Variable, float] = dataclasses.field(
        default_factory=dict
    )
    dual_values: Dict[linear_constraints.LinearConstraint, float] = dataclasses.field(
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
class ObjectiveParameters:
    """Parameters for an individual objective in a multi-objective model.

    This class mirrors (and can generate) the related proto
    model_parameters_pb2.ObjectiveParametersProto.

    Attributes:
      objective_degradation_absolute_tolerance: Optional objective degradation
        absolute tolerance. For a hierarchical multi-objective solver, each
        objective fⁱ is processed in priority order: the solver determines the
        optimal objective value Γⁱ, if it exists, subject to all constraints in
        the model and the additional constraints that fᵏ(x) = Γᵏ (within
        tolerances) for each k < i. If set, a solution is considered to be "within
        tolerances" for this objective fᵏ if |fᵏ(x) - Γᵏ| ≤
        `objective_degradation_absolute_tolerance`. See also
        `objective_degradation_relative_tolerance`; if both parameters are set for
        a given objective, the solver need only satisfy one to be considered
        "within tolerances".  If not None, must be nonnegative.
      objective_degradation_relative_tolerance: Optional objective degradation
        relative tolerance. For a hierarchical multi-objective solver, each
        objective fⁱ is processed in priority order: the solver determines the
        optimal objective value Γⁱ, if it exists, subject to all constraints in
        the model and the additional constraints that fᵏ(x) = Γᵏ (within
        tolerances) for each k < i. If set, a solution is considered to be "within
        tolerances" for this objective fᵏ if |fᵏ(x) - Γᵏ| ≤
        `objective_degradation_relative_tolerance` * |Γᵏ|. See also
        `objective_degradation_absolute_tolerance`; if both parameters are set for
        a given objective, the solver need only satisfy one to be considered
        "within tolerances". If not None, must be nonnegative.
      time_limit: The maximum time a solver should spend on optimizing this
        particular objective (or infinite if not set). Note that this does not
        supersede the global time limit in SolveParameters.time_limit; both will
        be enforced when set. This value is not a hard limit, solve time may
        slightly exceed this value.
    """

    objective_degradation_absolute_tolerance: Optional[float] = None
    objective_degradation_relative_tolerance: Optional[float] = None
    time_limit: Optional[datetime.timedelta] = None

    def to_proto(self) -> model_parameters_pb2.ObjectiveParametersProto:
        """Returns an equivalent protocol buffer."""
        result = model_parameters_pb2.ObjectiveParametersProto()
        if self.objective_degradation_absolute_tolerance is not None:
            result.objective_degradation_absolute_tolerance = (
                self.objective_degradation_absolute_tolerance
            )
        if self.objective_degradation_relative_tolerance is not None:
            result.objective_degradation_relative_tolerance = (
                self.objective_degradation_relative_tolerance
            )
        if self.time_limit is not None:
            result.time_limit.FromTimedelta(self.time_limit)
        return result


def parse_objective_parameters(
    proto: model_parameters_pb2.ObjectiveParametersProto,
) -> ObjectiveParameters:
    """Returns an equivalent ObjectiveParameters to the input proto."""
    result = ObjectiveParameters()
    if proto.HasField("objective_degradation_absolute_tolerance"):
        result.objective_degradation_absolute_tolerance = (
            proto.objective_degradation_absolute_tolerance
        )
    if proto.HasField("objective_degradation_relative_tolerance"):
        result.objective_degradation_relative_tolerance = (
            proto.objective_degradation_relative_tolerance
        )
    if proto.HasField("time_limit"):
        result.time_limit = proto.time_limit.ToTimedelta()
    return result


@dataclasses.dataclass
class ModelSolveParameters:
    """Model specific solver configuration, for example, an initial basis.

    This class mirrors (and can generate) the related proto
    model_parameters_pb2.ModelSolveParametersProto.

    Attributes:
      variable_values_filter: Only return solution and primal ray values for
        variables accepted by this filter (default accepts all variables).
      dual_values_filter: Only return dual variable values and dual ray values for
        linear constraints accepted by this filter (default accepts all linear
        constraints).
      quadratic_dual_values_filter: Only return quadratic constraint dual values
        accepted by this filter (default accepts all quadratic constraints).
      reduced_costs_filter: Only return reduced cost and dual ray values for
        variables accepted by this filter (default accepts all variables).
      initial_basis: If set, provides a warm start for simplex based solvers.
      solution_hints: Optional solution hints. If the underlying solver only
        accepts a single hint, the first hint is used.
      branching_priorities: Optional branching priorities. Variables with higher
        values will be branched on first. Variables for which priorities are not
        set get the solver's default priority (usually zero).
      objective_parameters: Optional per objective parameters used only only for
        multi-objective models.
      lazy_linear_constraints: Optional lazy constraint annotations. Included
        linear constraints will be marked as "lazy" with supporting solvers,
        meaning that they will only be added to the working model as-needed as the
        solver runs. Note that this an algorithmic hint that does not affect the
        model's feasible region; solvers not supporting these annotations will
        simply ignore it.
    """

    variable_values_filter: sparse_containers.VariableFilter = (
        sparse_containers.VariableFilter()
    )
    dual_values_filter: sparse_containers.LinearConstraintFilter = (
        sparse_containers.LinearConstraintFilter()
    )
    quadratic_dual_values_filter: sparse_containers.QuadraticConstraintFilter = (
        sparse_containers.QuadraticConstraintFilter()
    )
    reduced_costs_filter: sparse_containers.VariableFilter = (
        sparse_containers.VariableFilter()
    )
    initial_basis: Optional[solution.Basis] = None
    solution_hints: List[SolutionHint] = dataclasses.field(default_factory=list)
    branching_priorities: Dict[variables.Variable, int] = dataclasses.field(
        default_factory=dict
    )
    objective_parameters: Dict[objectives.Objective, ObjectiveParameters] = (
        dataclasses.field(default_factory=dict)
    )
    lazy_linear_constraints: Set[linear_constraints.LinearConstraint] = (
        dataclasses.field(default_factory=set)
    )

    def to_proto(self) -> model_parameters_pb2.ModelSolveParametersProto:
        """Returns an equivalent protocol buffer."""
        # TODO(b/236289022): these methods should check that the variables are from
        # the correct model.
        result = model_parameters_pb2.ModelSolveParametersProto(
            variable_values_filter=self.variable_values_filter.to_proto(),
            dual_values_filter=self.dual_values_filter.to_proto(),
            quadratic_dual_values_filter=self.quadratic_dual_values_filter.to_proto(),
            reduced_costs_filter=self.reduced_costs_filter.to_proto(),
            branching_priorities=sparse_containers.to_sparse_int32_vector_proto(
                self.branching_priorities
            ),
        )
        if self.initial_basis:
            result.initial_basis.CopyFrom(self.initial_basis.to_proto())
        for hint in self.solution_hints:
            result.solution_hints.append(hint.to_proto())
        for obj, params in self.objective_parameters.items():
            if isinstance(obj, objectives.AuxiliaryObjective):
                result.auxiliary_objective_parameters[obj.id].CopyFrom(
                    params.to_proto()
                )
            else:
                result.primary_objective_parameters.CopyFrom(params.to_proto())
        result.lazy_linear_constraint_ids[:] = sorted(
            con.id for con in self.lazy_linear_constraints
        )
        return result
