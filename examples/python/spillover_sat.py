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

"""Solves the problem of buying physical machines to meet VM demand.

The Spillover problem is defined as follows:

You have M types of physical machines and V types of Virtual Machines (VMs). You
can use a physical machine of type m to get n_mv copies of VM v. Each physical
machine m has a cost of c_m. Each VM has a demand of d_v. VMs are assigned to
physical machines by the following rule. The demand for each VM type arrives
equally spaced out over the interval [0, 1]. For each VM type, there is a
priority order over the physical machine types that you must follow. When a
demand arrives, if there are any machines of the highest priority type
available, you use them first, then you move on to the second priority machine
type, and so on. Each VM type has a list of compatible physical machine types,
and when the list is exhausted, the remaining demand is not met. Your goal is
to pick quantities of the physical machines to buy (minimizing cost) so that at
least some target service level (e.g. 95%) of the total demand of all VM is met.

The number of machines bought of each type and the number of VMs demanded of
each type is large enough that you can solve an approximate problem instead,
where the number of machines purchased and the assignment of machines to VMs is
fractional, if it is helpful to do so.

The problem is not particularly interesting in isolation, it is more interesting
to embed this LP inside a larger optimization problem (e.g. consider a two stage
problem where in stage one, you buy machines, then in stage two, you realize VM
demand).

The continuous approximation of this problem can be solved by LP (see the
MathOpt python examples). Doing this, instead of using MIP, is nontrivial.
Below, we show that continuous relaxation can be approximately solved by CP-SAT
as well, despite not having continuous variables. If you were solving the
problem in isolation, you should just use an LP solver, but if you were to add
side constraints or embed this within a more complex model, using CP-SAT could
be appropriate.

If for each VM type, the physical machines that are most cost effective are the
highest priority, AND the target service level is 100%, then the problem has a
trivial optimal solution:
  1. Rank the VMs by lowest cost to meet a unit of demand with the #1 preferred
     machine type.
  2. For each VM type in the order above, buy machines from #1 preferred machine
     type, until either you have met all demand for the VM type.

MOE:begin_strip
This example is motivated by the Cloudy problem, see go/fluid-model.
MOE:end_strip
"""

from collections.abc import Sequence
import dataclasses
import math
import random

from absl import app
from absl import flags
from ortools.sat.python import cp_model

_MACHINE_TYPES = flags.DEFINE_integer(
    "machine_types",
    100,
    "How many types of machines we can fulfill demand with.",
)

_VM_TYPES = flags.DEFINE_integer(
    "vm_types", 500, "How many types of VMs we need to supply."
)

_FUNGIBILITY = flags.DEFINE_integer(
    "fungibility",
    10,
    "Each VM type can be satisfied with this many machine types, selected"
    " uniformly at random.",
)

_MAX_DEMAND = flags.DEFINE_integer(
    "max_demand",
    100,
    "Demand for each VM type is in [max_demand//2, max_demand], uniformly at"
    " random.",
)

_TEST_DATA = flags.DEFINE_bool(
    "test_data", False, "Use small test instance instead of random data."
)

_SEED = flags.DEFINE_integer("seed", 13, "RNG seed for instance creation.")

_TIME_STEPS = flags.DEFINE_integer("time_steps", 100, "How much to discretize time.")


@dataclasses.dataclass(frozen=True)
class MachineUse:
    machine_type: int
    vms_per_machine: int


@dataclasses.dataclass(frozen=True)
class VmDemand:
    compatible_machines: tuple[MachineUse, ...]
    vm_quantity: int


@dataclasses.dataclass(frozen=True)
class SpilloverProblem:
    machine_cost: tuple[float, ...]
    machine_limit: tuple[int, ...]
    vm_demands: tuple[VmDemand, ...]
    service_level: float
    time_horizon: int


def _random_spillover_problem(
    num_machines: int,
    num_vms: int,
    fungibility: int,
    max_vm_demand: int,
    horizon: int,
) -> SpilloverProblem:
    """Generates a random SpilloverProblem."""
    machine_costs = tuple(random.random() for _ in range(num_machines))
    vm_demands = []
    all_machines = list(range(num_machines))
    min_vm_demand = max_vm_demand // 2
    for _ in range(num_vms):
        vm_use = []
        for machine in random.sample(all_machines, fungibility):
            vm_use.append(
                MachineUse(machine_type=machine, vms_per_machine=random.randint(1, 10))
            )
        vm_demands.append(
            VmDemand(
                compatible_machines=tuple(vm_use),
                vm_quantity=random.randint(min_vm_demand, max_vm_demand),
            )
        )
    machine_need_ub = num_vms * max_vm_demand
    machine_limit = (machine_need_ub,) * num_machines
    return SpilloverProblem(
        machine_cost=machine_costs,
        machine_limit=machine_limit,
        vm_demands=tuple(vm_demands),
        service_level=0.95,
        time_horizon=horizon,
    )


def _test_problem() -> SpilloverProblem:
    """Creates a small SpilloverProblem with optimal objective of 360."""
    # To avoid machine type 2, ensure we buy enough of 1 to not stock out, cost
    # 20
    vm_a = VmDemand(
        vm_quantity=10,
        compatible_machines=(
            MachineUse(machine_type=1, vms_per_machine=1),
            MachineUse(machine_type=2, vms_per_machine=1),
        ),
    )
    # machine type 0 is cheaper, but we don't want to stock out of machine type 1,
    # so use all machine type 1, cost 40.
    vm_b = VmDemand(
        vm_quantity=20,
        compatible_machines=(
            MachineUse(machine_type=1, vms_per_machine=1),
            MachineUse(machine_type=0, vms_per_machine=1),
        ),
    )
    # Will use 3 copies of machine type 2, cost 300
    vm_c = VmDemand(
        vm_quantity=30,
        compatible_machines=(MachineUse(machine_type=2, vms_per_machine=10),),
    )
    return SpilloverProblem(
        machine_cost=(1.0, 2.0, 100.0),
        machine_limit=(60, 60, 60),
        vm_demands=(vm_a, vm_b, vm_c),
        service_level=1.0,
        time_horizon=100,
    )


# Indices:
#  * i in I, the VM demands
#  * j in J, the machines supplied
#
# Data:
#  * c_j: cost of a machine of type j
#  * l_j: a limit of how many machines of type j you can buy.
#  * n_ij: how many VMs of type i you get from a machine of type j
#  * d_i: the total demand for VMs of type i
#  * service_level: the target fraction of demand that is met.
#  * P_i subset J: the compatible machine types for VM demand i.
#  * UP_i(j) subset P_i, for j in P_i: for VM demand type i, the machines of
#    priority higher than j
#  * T: the number of integer time steps.
#
# Note: when d_i/n_ij is not integer, some approximation error is introduced in
# constraint 6 below.
#
# Decision variables:
#  * s_j: the supply of machine type j
#  * w_j: the time we run out of machine j, or 1 if we never run out
#  * v_ij: when we start using supply j to meet demand i, or w_j if we never use
#          this machine type for this demand.
#  * o_i: the time we start failing to meet vm demand i
#  * m_i: the total demand met for vm type i.
#
# Model the problem:
#   min   sum_{j in J} c_j s_j
#   s.t.
#     1:  sum_i m_i >= service_level * sum_{i in I} d_i
#     2:  T * m_i <= o_i * d_i                                    for all i in I
#     3:  v_ij >= w_r                     for all i in I, j in C_i, r in UP_i(j)
#     4:  v_ij <= w_j                                   for all i in I, j in C_i
#     5:  o_i = sum_{j in P_i} (w_j - v_ij)                       for all i in I
#     6:  sum_{i in I: j in P_i}ceil(d_i/n_ij)(w_j - v_ij)<=T*s_j for all j in J
#         o_i, w_j, v_ij in [0, T]
#         0 <= m_i <= d_i
#         0 <= s_j <= l_j
#
# The constraints say:
#  1. The amount of demand served must be at least 95% of total demand.
#  2. The demand served for VM type i is linear in the time we fail to keep
#     serving demand.
#  3. Don't start using machine type j for demand i until all higher priority
#     machine types r are used up.
#  4. The time we run out of machine type j must be after we start using it for
#     VM demand type i.
#  5. The time we are unable to serve further VM demand i is the sum of the
#     time spent serving the demand with each eligible machine type.
#  6. The total use of machine type j to serve demand does not exceed the
#     supply. The ceil function above introduces some approximation error when
#     d_i/n_ij is not integer.
def _solve_spillover_problem(problem: SpilloverProblem) -> None:
    """Solves the spillover problem and prints the optimal objective."""
    model = cp_model.CpModel()
    num_machines = len(problem.machine_cost)
    num_vms = len(problem.vm_demands)
    horizon = problem.time_horizon
    s = [
        model.new_int_var(lb=0, ub=problem.machine_limit[j], name=f"s_{j}")
        for j in range(num_machines)
    ]
    w = [
        model.new_int_var(lb=0, ub=horizon, name=f"w_{i}") for i in range(num_machines)
    ]
    o = [model.new_int_var(lb=0, ub=horizon, name=f"o_{j}") for j in range(num_vms)]
    m = [
        model.new_int_var(lb=0, ub=problem.vm_demands[j].vm_quantity, name=f"m_{j}")
        for j in range(num_vms)
    ]
    v = [
        {
            compat.machine_type: model.new_int_var(
                lb=0, ub=horizon, name=f"v_{i}_{compat.machine_type}"
            )
            for compat in vm_demand.compatible_machines
        }
        for i, vm_demand in enumerate(problem.vm_demands)
    ]

    obj = 0
    for j in range(num_machines):
        obj += s[j] * problem.machine_cost[j]
    model.minimize(obj)

    # Constraint 1: demand served is at least service_level fraction of total.
    total_vm_demand = sum(vm_demand.vm_quantity for vm_demand in problem.vm_demands)
    model.add(sum(m) >= int(math.ceil(problem.service_level * total_vm_demand)))

    # Constraint 2: demand served is linear in time we stop serving.
    for i in range(num_vms):
        model.add(
            problem.time_horizon * m[i] <= o[i] * problem.vm_demands[i].vm_quantity
        )

    # Constraint 3: use machine type j for demand i after all higher priority
    # machine types r are used up.
    for i in range(num_vms):
        for k, meet_demand in enumerate(problem.vm_demands[i].compatible_machines):
            j = meet_demand.machine_type
            for l in range(k):
                r = problem.vm_demands[i].compatible_machines[l].machine_type
                model.add(v[i][j] >= w[r])

    # Constraint 4: outage time of machine j is after start time for using j to
    # meet VM demand i.
    for i in range(num_vms):
        for meet_demand in problem.vm_demands[i].compatible_machines:
            j = meet_demand.machine_type
            model.add(v[i][j] <= w[j])

    # Constraint 5: For VM demand i, time service ends is the sum of the time
    # spent serving with each eligible machine type.
    for i in range(num_vms):
        sum_serving = 0
        for meet_demand in problem.vm_demands[i].compatible_machines:
            j = meet_demand.machine_type
            sum_serving += w[j] - v[i][j]
        model.add(o[i] == sum_serving)

    # Constraint 6: Total use of machine type j is at most the supply.
    #
    # We build the constraints in bulk because our data is transposed.
    total_machine_use = [0 for _ in range(num_machines)]
    for i in range(num_vms):
        for meet_demand in problem.vm_demands[i].compatible_machines:
            j = meet_demand.machine_type
            nij = meet_demand.vms_per_machine
            vm_quantity = problem.vm_demands[i].vm_quantity
            # Want vm_quantity/nij, over estimate with ceil(vm_quantity/nij) to use
            # integer coefficients.
            rate = (vm_quantity + nij - 1) // nij
            total_machine_use[j] += rate * (w[j] - v[i][j])
    for j in range(num_machines):
        model.add(total_machine_use[j] <= horizon * s[j])

    solver = cp_model.CpSolver()
    solver.parameters.num_workers = 16
    solver.parameters.log_search_progress = True
    solver.max_time_in_seconds = 30.0
    status = solver.solve(model)
    if status != cp_model.OPTIMAL:
        raise RuntimeError(f"expected optimal, found: {status}")
    print(f"objective: {solver.objective_value}")


def main(argv: Sequence[str]) -> None:
    del argv  # Unused.
    random.seed(_SEED.value)
    if _TEST_DATA.value:
        problem = _test_problem()
    else:
        problem = _random_spillover_problem(
            _MACHINE_TYPES.value,
            _VM_TYPES.value,
            _FUNGIBILITY.value,
            _MAX_DEMAND.value,
            _TIME_STEPS.value,
        )
    print(problem)

    _solve_spillover_problem(problem)


if __name__ == "__main__":
    app.run(main)
