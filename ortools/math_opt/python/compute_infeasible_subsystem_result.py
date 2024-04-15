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

"""Data types for the result of calling `mathopt.compute_infeasible_subsystem."""

import dataclasses
from typing import FrozenSet, Mapping

import immutabledict

from ortools.math_opt import infeasible_subsystem_pb2
from ortools.math_opt.python import model
from ortools.math_opt.python import result


@dataclasses.dataclass(frozen=True)
class ModelSubsetBounds:
    """Presence of the upper and lower bounds in a two-sided constraint.

    E.g. for 1 <= x <= 2, `lower` is the constraint 1 <= x and `upper` is the
    constraint x <= 2.

    Attributes:
      lower: If the lower bound half of the two-sided constraint is selected.
      upper: If the upper bound half of the two-sided constraint is selected.
    """

    lower: bool = False
    upper: bool = False

    def empty(self) -> bool:
        """Is empty if both `lower` and `upper` are False."""
        return not (self.lower or self.upper)

    def to_proto(self) -> infeasible_subsystem_pb2.ModelSubsetProto.Bounds:
        """Returns an equivalent proto message for these bounds."""
        return infeasible_subsystem_pb2.ModelSubsetProto.Bounds(
            lower=self.lower, upper=self.upper
        )


def parse_model_subset_bounds(
    bounds: infeasible_subsystem_pb2.ModelSubsetProto.Bounds,
) -> ModelSubsetBounds:
    """Returns an equivalent `ModelSubsetBounds` to the input proto."""
    return ModelSubsetBounds(lower=bounds.lower, upper=bounds.upper)


@dataclasses.dataclass(frozen=True)
class ModelSubset:
    """A subset of a Model's constraints (including variable bounds/integrality).

    When returned from `solve.compute_infeasible_subsystem`, the contained
    `ModelSubsetBounds` will all be nonempty.

    Attributes:
      variable_bounds: The upper and/or lower bound constraints on these variables
        are included in the subset.
      variable_integrality: The constraint that a variable is integer is included
        in the subset.
      linear_constraints: The upper and/or lower bounds from these linear
        constraints are included in the subset.
    """

    variable_bounds: Mapping[model.Variable, ModelSubsetBounds] = (
        immutabledict.immutabledict()
    )
    variable_integrality: FrozenSet[model.Variable] = frozenset()
    linear_constraints: Mapping[model.LinearConstraint, ModelSubsetBounds] = (
        immutabledict.immutabledict()
    )

    def empty(self) -> bool:
        """Returns true if all the nested constraint collections are empty.

        Warning: When `self.variable_bounds` or `self.linear_constraints` contain
        only ModelSubsetBounds which are themselves empty, this function will return
        False.

        Returns:
          True if this is empty.
        """
        return not (
            self.variable_bounds or self.variable_integrality or self.linear_constraints
        )

    def to_proto(self) -> infeasible_subsystem_pb2.ModelSubsetProto:
        """Returns an equivalent proto message for this `ModelSubset`."""
        return infeasible_subsystem_pb2.ModelSubsetProto(
            variable_bounds={
                var.id: bounds.to_proto()
                for (var, bounds) in self.variable_bounds.items()
            },
            variable_integrality=sorted(var.id for var in self.variable_integrality),
            linear_constraints={
                con.id: bounds.to_proto()
                for (con, bounds) in self.linear_constraints.items()
            },
        )


def parse_model_subset(
    model_subset: infeasible_subsystem_pb2.ModelSubsetProto, mod: model.Model
) -> ModelSubset:
    """Returns an equivalent `ModelSubset` to the input proto."""
    if model_subset.quadratic_constraints:
        raise NotImplementedError(
            "quadratic_constraints not yet implemented for ModelSubset in Python"
        )
    if model_subset.second_order_cone_constraints:
        raise NotImplementedError(
            "second_order_cone_constraints not yet implemented for ModelSubset in"
            " Python"
        )
    if model_subset.sos1_constraints:
        raise NotImplementedError(
            "sos1_constraints not yet implemented for ModelSubset in Python"
        )
    if model_subset.sos2_constraints:
        raise NotImplementedError(
            "sos2_constraints not yet implemented for ModelSubset in Python"
        )
    if model_subset.indicator_constraints:
        raise NotImplementedError(
            "indicator_constraints not yet implemented for ModelSubset in Python"
        )
    return ModelSubset(
        variable_bounds={
            mod.get_variable(var_id): parse_model_subset_bounds(bounds)
            for var_id, bounds in model_subset.variable_bounds.items()
        },
        variable_integrality=frozenset(
            mod.get_variable(var_id) for var_id in model_subset.variable_integrality
        ),
        linear_constraints={
            mod.get_linear_constraint(con_id): parse_model_subset_bounds(bounds)
            for con_id, bounds in model_subset.linear_constraints.items()
        },
    )


@dataclasses.dataclass(frozen=True)
class ComputeInfeasibleSubsystemResult:
    """The result of searching for an infeasible subsystem.

    This is the result of calling `mathopt.compute_infeasible_subsystem()`.

    Attributes:
      feasibility: If the problem was proven feasible, infeasible, or no
        conclusion was reached. The fields below are ignored unless the problem
        was proven infeasible.
      infeasible_subsystem: Ignored unless `feasibility` is `INFEASIBLE`, a subset
        of the model that is still infeasible.
      is_minimal: Ignored unless `feasibility` is `INFEASIBLE`. If True, then the
        removal of any constraint from `infeasible_subsystem` makes the sub-model
        feasible. Note that, due to problem transformations MathOpt applies or
        idiosyncrasies of the solvers contract, the returned infeasible subsystem
        may not actually be minimal.
    """

    feasibility: result.FeasibilityStatus = result.FeasibilityStatus.UNDETERMINED
    infeasible_subsystem: ModelSubset = ModelSubset()
    is_minimal: bool = False

    def to_proto(
        self,
    ) -> infeasible_subsystem_pb2.ComputeInfeasibleSubsystemResultProto:
        """Returns an equivalent proto for this `ComputeInfeasibleSubsystemResult`."""
        return infeasible_subsystem_pb2.ComputeInfeasibleSubsystemResultProto(
            feasibility=self.feasibility.value,
            infeasible_subsystem=self.infeasible_subsystem.to_proto(),
            is_minimal=self.is_minimal,
        )


def parse_compute_infeasible_subsystem_result(
    infeasible_system_result: infeasible_subsystem_pb2.ComputeInfeasibleSubsystemResultProto,
    mod: model.Model,
) -> ComputeInfeasibleSubsystemResult:
    """Returns an equivalent `ComputeInfeasibleSubsystemResult` to the input proto."""
    return ComputeInfeasibleSubsystemResult(
        feasibility=result.FeasibilityStatus(infeasible_system_result.feasibility),
        infeasible_subsystem=parse_model_subset(
            infeasible_system_result.infeasible_subsystem, mod
        ),
        is_minimal=infeasible_system_result.is_minimal,
    )
