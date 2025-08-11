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

"""An advanced benders decomposition example.

We consider a network design problem where each location has a demand that
must be met by its neighboring facilities, and each facility can control
its total capacity. In this version we also require that locations cannot
use more that a specified fraction of a facilities capacity.

Problem data:
* F: set of facilities.
* L: set of locations.
* E: subset of {(f,l) : f in F, l in L} that describes the network between
     facilities and locations.
* d: demand at location (all demands are equal for simplicity).
* c: cost per unit of capacity at a facility (all facilities are have the
     same cost for simplicity).
* h: cost per unit transported through an edge.
* a: fraction of a facility's capacity that can be used by each location.

Decision variables:
* z_f: capacity at facility f in F.
* x_(f,l): flow from facility f to location l for all (f,l) in E.

Formulation:

  min c * sum(z_f : f in F) + sum(h_e * x_e : e in E)
  s.t.
                                  x_(f,l) <= a * z_f   for all (f,l) in E
    sum(x_(f,l) : l such that (f,l) in E) <=     z_f   for all f in F
    sum(x_(f,l) : f such that (f,l) in E) >= d     for all l in L
                                      x_e >= 0     for all e in E
                                      z_f >= 0     for all f in F

Below we solve this problem directly and using a benders decompostion
approach.
"""
from collections.abc import Sequence
import dataclasses
import math
import time

from absl import app
from absl import flags
import numpy as np

from ortools.math_opt.python import mathopt

_NUM_FACILITIES = flags.DEFINE_integer("num_facilities", 3000, "Number of facilities.")
_NUM_LOCATIONS = flags.DEFINE_integer("num_locations", 50, "Number of locations.")
_EDGE_PROBABILITY = flags.DEFINE_float("edge_probability", 0.99, "Edge probability.")
_BENDERS_PRECISSION = flags.DEFINE_float(
    "benders_precission", 1e-9, "Benders target precission."
)
_LOCATION_DEMAND = flags.DEFINE_float("location_demand", 1, "Client demands.")
_FACILITY_COST = flags.DEFINE_float("facility_cost", 100, "Facility capacity cost.")
_LOCATION_FRACTION = flags.DEFINE_float(
    "location_fraction",
    0.001,
    "Fraction of a facility's capacity that can be used by each location.",
)
_SOLVER_TYPE = flags.DEFINE_enum_class(
    "solver_type",
    mathopt.SolverType.GLOP,
    mathopt.SolverType,
    "the LP solver to use, possible values: glop, gurobi, glpk, pdlp.",
)

_ZERO_TOL = 1.0e-3

################################################################################
# Facility location instance representation and generation
################################################################################

# First element is a facility and second is a location.
Edge = tuple[int, int]


class Network:
    """A simple randomly-generated facility-location network."""

    def __init__(
        self, num_facilities: int, num_locations: int, edge_probability: float
    ) -> None:
        rng = np.random.default_rng(123)
        self.num_facilities: int = num_facilities
        self.num_locations: int = num_locations
        self.facility_edge_incidence: list[list[Edge]] = [
            [] for facility in range(num_facilities)
        ]
        self.location_edge_incidence: list[list[Edge]] = [
            [] for location in range(num_locations)
        ]
        self.edges: list[Edge] = []
        self.edge_costs: dict[Edge, float] = {}
        for facility in range(num_facilities):
            for location in range(num_locations):
                if rng.binomial(1, edge_probability):
                    edge: Edge = (facility, location)
                    self.facility_edge_incidence[facility].append(edge)
                    self.location_edge_incidence[location].append(edge)
                    self.edges.append(edge)
                    self.edge_costs[edge] = rng.uniform()

        # Ensure every facility is connected to at least one location and every
        # location is connected to at least one facility.
        for facility in range(num_facilities):
            if not self.facility_edge_incidence[facility]:
                location = rng.integer(num_locations)
                edge: Edge = (facility, location)
                self.facility_edge_incidence[facility].append(edge)
                self.location_edge_incidence[location].append(edge)
                self.edges.append(edge)
                self.edge_costs[edge] = rng.uniform()
        for location in range(num_locations):
            if not self.location_edge_incidence[location]:
                facility = rng.integer(num_facilities)
                edge: Edge = (facility, location)
                self.facility_edge_incidence[facility].append(edge)
                self.location_edge_incidence[location].append(edge)
                self.edges.append(edge)
                self.edge_costs[edge] = rng.uniform()


@dataclasses.dataclass
class FacilityLocationInstance:
    """A facility location instance."""

    network: Network
    location_demand: float
    facility_cost: float
    location_fraction: float


################################################################################
# Direct solve
################################################################################


def full_problem(
    instance: FacilityLocationInstance, solver_type: mathopt.SolverType
) -> None:
    """Solves the full formulation of the facility location problem.

    See file level comment for problem description and formulation.

    Args:
      instance: a facility location instance.
      solver_type: what solver to use.

    Raises:
      RuntimeError: On solve errors.
    """
    num_facilities = instance.network.num_facilities
    num_locations = instance.network.num_locations
    model = mathopt.Model(name="Full network design problem")

    # Capacity variables
    z = [model.add_variable(lb=0.0) for j in range(num_facilities)]

    # Flow variables
    x = {edge: model.add_variable(lb=0.0) for edge in instance.network.edges}

    # Objective Function
    objective_for_edges = sum(
        instance.network.edge_costs[edge] * x[edge] for edge in instance.network.edges
    )
    model.minimize(objective_for_edges + instance.facility_cost * sum(z))

    # Demand constraints
    for location in range(num_locations):
        incoming_supply = sum(
            x[edge] for edge in instance.network.location_edge_incidence[location]
        )
        model.add_linear_constraint(incoming_supply >= instance.location_demand)

    # Supply constraints
    for facility in range(num_facilities):
        outgoing_supply = sum(
            x[edge] for edge in instance.network.facility_edge_incidence[facility]
        )
        model.add_linear_constraint(outgoing_supply <= z[facility])

    # Arc constraints
    for facility in range(num_facilities):
        for edge in instance.network.facility_edge_incidence[facility]:
            model.add_linear_constraint(
                x[edge] <= instance.location_fraction * z[facility]
            )

    result = mathopt.solve(model, solver_type)
    if result.termination.reason != mathopt.TerminationReason.OPTIMAL:
        raise RuntimeError(f"failed to find an optimal solution: {result.termination}")
    print(f"Fulll problem optimal objective: {result.objective_value():.9f}")


################################################################################
# Benders solver
################################################################################


# Setup first stage model:
#
#   min c * sum(z_f : f in F) + w
#   s.t.
#                                       z_f >= 0     for all f in F
#          sum(fcut_f^i z_f) + fcut_const^i <= 0      for i = 1,...
#          sum(ocut_f^j z_f) + ocut_const^j <= w      for j = 1,...
class FirstStageProblem:
    """First stage model and variables."""

    model: mathopt.Model
    z: list[mathopt.Variable]
    w: mathopt.Variable

    def __init__(self, network: Network, facility_cost: float) -> None:
        self.model: mathopt.Model = mathopt.Model(name="First stage problem")

        # Capacity variables
        self.z: list[mathopt.Variable] = [
            self.model.add_variable(lb=0.0) for j in range(network.num_facilities)
        ]

        # Objective variable
        self.w: mathopt.Variable = self.model.add_variable(lb=0.0)

        # First stage objective
        self.model.minimize(self.w + facility_cost * sum(self.z))


@dataclasses.dataclass
class Cut:
    """A feasibility or optimality cut.

    The cut is of the form:

      z_coefficients^T z + constant <= w_coefficient * w

    This will be a feasibility cut if w_coefficient = 0.0 and an optimality
    cut if w_coefficient = 1.
    """

    z_coefficients: list[float] = dataclasses.field(default_factory=list)
    constant: float = 0.0
    w_coefficient: float = 0.0


def ensure_dual_ray_solve_parameters(
    solver_type: mathopt.SolverType,
) -> mathopt.SolveParameters:
    """Set parameters to ensure a dual ray is returned for infeasible problems."""
    if solver_type == mathopt.SolverType.GUROBI:
        return mathopt.SolveParameters(
            gurobi=mathopt.GurobiParameters(param_values={"InfUnbdInfo": "1"})
        )
    if solver_type == mathopt.SolverType.GLOP:
        return mathopt.SolveParameters(
            presolve=mathopt.Emphasis.OFF,
            scaling=mathopt.Emphasis.OFF,
            lp_algorithm=mathopt.LPAlgorithm.DUAL_SIMPLEX,
        )
    if solver_type == mathopt.SolverType.GLPK:
        return mathopt.SolveParameters(
            presolve=mathopt.Emphasis.OFF,
            lp_algorithm=mathopt.LPAlgorithm.DUAL_SIMPLEX,
            glpk=mathopt.GlpkParameters(compute_unbound_rays_if_possible=True),
        )
    if solver_type == mathopt.SolverType.PDLP:
        # No arguments needed.
        return mathopt.SolveParameters()
    raise ValueError(f"unsupported solver: {solver_type}")


class SecondStageSolver:
    """Solves the second stage model.

      The model is:

      min sum(h_e * x_e : e in E)
      s.t.
                                      x_(f,l) <= a * zz_f   for all (f,l) in E
        sum(x_(f,l) : l such that (f,l) in E) <=     zz_f   for all f in F
        sum(x_(f,l) : f such that (f,l) in E) >= d     for all l in L
                                          x_e >= 0     for all e in E

    where zz_f are fixed values for z_f from the first stage model, and generates
    an infeasibility or optimality cut as needed.
    """

    def __init__(
        self, instance: FacilityLocationInstance, solver_type: mathopt.SolverType
    ) -> None:
        self._network: Network = instance.network
        self._second_stage_params: mathopt.SolveParameters = (
            ensure_dual_ray_solve_parameters(solver_type)
        )
        self._location_fraction: float = instance.location_fraction

        num_facilities = self._network.num_facilities
        num_locations = self._network.num_locations
        self._second_stage_model = mathopt.Model(name="Second stage model")

        # Flow variables
        self._x = {
            edge: self._second_stage_model.add_variable(lb=0.0)
            for edge in self._network.edges
        }

        # Objective Function
        objective_for_edges = sum(
            self._network.edge_costs[edge] * self._x[edge]
            for edge in self._network.edges
        )
        self._second_stage_model.minimize(objective_for_edges)

        # Demand constraints
        self._demand_constraints: list[mathopt.LinearConstraint] = []
        for location in range(num_locations):
            incoming_supply = sum(
                self._x[edge]
                for edge in self._network.location_edge_incidence[location]
            )
            self._demand_constraints.append(
                self._second_stage_model.add_linear_constraint(
                    incoming_supply >= instance.location_demand
                )
            )

        # Supply constraints
        self._supply_constraint: list[mathopt.LinearConstraint] = []
        for facility in range(num_facilities):
            outgoing_supply = sum(
                self._x[edge]
                for edge in self._network.facility_edge_incidence[facility]
            )
            # Set supply constraint with trivial upper bound to be updated with first
            # stage information.
            self._supply_constraint.append(
                self._second_stage_model.add_linear_constraint(
                    outgoing_supply <= math.inf
                )
            )

        self._second_stage_solver = mathopt.IncrementalSolver(
            self._second_stage_model, solver_type
        )

    def solve(
        self, z_values: list[float], w_value: float, first_stage_objective: float
    ) -> tuple[float, Cut]:
        """Solve the second stage problem."""
        num_facilities = self._network.num_facilities
        # Update second stage with first stage solution.
        for facility in range(num_facilities):
            if z_values[facility] < -_ZERO_TOL:
                raise RuntimeError(
                    "negative z_value in first stage: "
                    f"{z_values[facility]} for facility {facility}"
                )
            # Make sure variable bounds are valid (lb <= ub).
            capacity_value = max(z_values[facility], 0.0)
            for edge in self._network.facility_edge_incidence[facility]:
                self._x[edge].upper_bound = self._location_fraction * capacity_value
            self._supply_constraint[facility].upper_bound = capacity_value
        # Solve and process second stage.
        second_stage_result = self._second_stage_solver.solve(
            params=self._second_stage_params
        )
        if second_stage_result.termination.reason not in (
            mathopt.TerminationReason.OPTIMAL,
            mathopt.TerminationReason.INFEASIBLE,
        ):
            raise RuntimeError(
                "second stage was not solved to optimality or infeasibility: "
                f"{second_stage_result.termination}"
            )
        if (
            second_stage_result.termination.reason
            == mathopt.TerminationReason.INFEASIBLE
        ):
            # If the second stage problem is infeasible we can construct a
            # feasibility cut from a returned dual ray.
            return math.inf, self.feasibility_cut(second_stage_result)
        else:
            # If the second stage problem is optimal we can construct an optimality
            # cut from a returned dual optimal solution. We can also update the upper
            # bound, which is obtained by switching predicted second stage objective
            # value w with the true second stage objective value.
            upper_bound = (
                first_stage_objective - w_value + second_stage_result.objective_value()
            )
            return upper_bound, self.optimality_cut(second_stage_result)

    def feasibility_cut(self, second_stage_result: mathopt.SolveResult) -> Cut:
        """Build and return a feasibility cut.

        If the second stage problem is infeasible we get a dual ray (r, y) such
        that

        sum(r_(f,l)*a*zz_f : (f,l) in E, r_(f,l) < 0)
        + sum(y_f*zz_f : f in F, y_f < 0)
        + sum(y_l*d : l in L, y_l > 0) > 0.

        Then we get the feasibility cut (go/math_opt-advanced-dual-use#benders)

        sum(fcut_f*z_f) + fcut_const <= 0,

        where

        fcut_f     = sum(r_(f,l)*a : (f,l) in E, r_(f,l) < 0)
                     + min{y_f, 0}
        fcut_const = sum*(y_l*d : l in L, y_l > 0)

        Args:
          second_stage_result: The result from the second stage solve.

        Raises:
          RuntimeError: if dual ray is not available.

        Returns:
          A feasibility cut.
        """
        num_facilities = self._network.num_facilities
        result = Cut()
        if not second_stage_result.has_dual_ray():
            # MathOpt does not require solvers to return a dual ray on infeasible,
            # but most LP solvers always will, see go/mathopt-solver-contracts for
            # details.
            raise RuntimeError("no dual ray available for feasibility cut")

        for facility in range(num_facilities):
            coefficient = 0.0
            for edge in self._network.facility_edge_incidence[facility]:
                reduced_cost = second_stage_result.ray_reduced_costs(self._x[edge])
                coefficient += self._location_fraction * min(reduced_cost, 0.0)
            dual_value = second_stage_result.ray_dual_values(
                self._supply_constraint[facility]
            )
            coefficient += min(dual_value, 0.0)
            result.z_coefficients.append(coefficient)
        result.constant = 0.0
        for constraint in self._demand_constraints:
            dual_value = second_stage_result.ray_dual_values(constraint)
            result.constant += max(dual_value, 0.0)
        result.w_coefficient = 0.0
        return result

    def optimality_cut(self, second_stage_result: mathopt.SolveResult) -> Cut:
        """Build and return an optimality cut.

        If the second stage problem is optimal we get a dual solution (r, y) such
        that the optimal objective value is equal to

        sum(r_(f,l)*a*zz_f : (f,l) in E, r_(f,l) < 0)
        + sum(y_f*zz_f : f in F, y_f < 0)
        + sum*(y_l*d : l in L, y_l > 0) > 0.

        Then we get the optimality cut (go/math_opt-advanced-dual-use#benders)

        sum(ocut_f*z_f) + ocut_const <= w,

        where

        ocut_f     = sum(r_(f,l)*a : (f,l) in E, r_(f,l) < 0)
                     + min{y_f, 0}
        ocut_const = sum*(y_l*d : l in L, y_l > 0)

        Args:
          second_stage_result: The result from the second stage solve.

        Raises:
          RuntimeError: if dual solution is not available.

        Returns:
          An optimality cut.
        """
        num_facilities = self._network.num_facilities
        result = Cut()
        if not second_stage_result.has_dual_feasible_solution():
            # MathOpt does not require solvers to return a dual solution on optimal,
            # but most LP solvers always will, see go/mathopt-solver-contracts for
            # details.
            raise RuntimeError("no dual solution available for optimality cut")
        for facility in range(num_facilities):
            coefficient = 0.0
            for edge in self._network.facility_edge_incidence[facility]:
                reduced_cost = second_stage_result.reduced_costs(self._x[edge])
                coefficient += self._location_fraction * min(reduced_cost, 0.0)
            dual_value = second_stage_result.dual_values(
                self._supply_constraint[facility]
            )
            coefficient += min(dual_value, 0.0)
            result.z_coefficients.append(coefficient)
        result.constant = 0.0
        for constraint in self._demand_constraints:
            dual_value = second_stage_result.dual_values(constraint)
            result.constant += max(dual_value, 0.0)
        result.w_coefficient = 1.0
        return result


def benders(
    instance: FacilityLocationInstance,
    target_precission: float,
    solver_type: mathopt.SolverType,
    maximum_iterations: int = 30000,
) -> None:
    """Solves the facility location problem using Benders decomposition.

    Args:
      instance: a facility location instance.
      target_precission: the target difference between upper and lower bounds.
      solver_type: what solver to use.
      maximum_iterations: limit on the number of iterations.

    Raises:
      RuntimeError: On solve errors.
    """
    num_facilities = instance.network.num_facilities

    # Setup first stage model and solver.
    first_stage = FirstStageProblem(instance.network, instance.facility_cost)
    first_stage_solver = mathopt.IncrementalSolver(first_stage.model, solver_type)

    # Setup second stage solver.
    second_stage_solver = SecondStageSolver(instance, solver_type)

    # Start Benders
    iteration = 0
    best_upper_bound = math.inf
    while True:
        print(f"Iteration: {iteration}", flush=True)

        # Solve and process first stage.
        first_stage_result = first_stage_solver.solve()
        if first_stage_result.termination.reason != mathopt.TerminationReason.OPTIMAL:
            raise RuntimeError(
                "could not solve first stage problem to optimality: "
                f"{first_stage_result.termination}"
            )
        z_values = [
            first_stage_result.variable_values(first_stage.z[j])
            for j in range(num_facilities)
        ]
        lower_bound = first_stage_result.objective_value()
        print(f"LB = {lower_bound}", flush=True)
        # Solve and process second stage.
        upper_bound, cut = second_stage_solver.solve(
            z_values,
            first_stage_result.variable_values(first_stage.w),
            first_stage_result.objective_value(),
        )
        cut_expression = sum(
            cut.z_coefficients[j] * first_stage.z[j] for j in range(num_facilities)
        )
        cut_expression += cut.constant
        first_stage.model.add_linear_constraint(
            cut_expression <= cut.w_coefficient * first_stage.w
        )
        best_upper_bound = min(upper_bound, best_upper_bound)
        print(f"UB = {best_upper_bound}", flush=True)
        iteration += 1
        if best_upper_bound - lower_bound < target_precission:
            print(f"Total iterations = {iteration}", flush=True)
            print(f"Final LB = {lower_bound:.9f}", flush=True)
            print(f"Final UB = {best_upper_bound:.9f}", flush=True)
            break
        if iteration > maximum_iterations:
            break


def main(argv: Sequence[str]) -> None:
    del argv  # Unused.
    instance = FacilityLocationInstance(
        network=Network(
            _NUM_FACILITIES.value, _NUM_LOCATIONS.value, _EDGE_PROBABILITY.value
        ),
        location_demand=_LOCATION_DEMAND.value,
        facility_cost=_FACILITY_COST.value,
        location_fraction=_LOCATION_FRACTION.value,
    )
    start = time.monotonic()
    full_problem(instance, _SOLVER_TYPE.value)
    print(f"Full solve time: {time.monotonic() - start}s")
    start = time.monotonic()
    benders(instance, _BENDERS_PRECISSION.value, _SOLVER_TYPE.value)
    print(f"Benders solve time: {time.monotonic() - start}s")


if __name__ == "__main__":
    app.run(main)
