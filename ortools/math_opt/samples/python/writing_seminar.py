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

"""Solves the problem of assigning students to classes to maximize welfare.

Each student provides a ranking of their top five preferred writing seminars to
be in, and can be assigned to at most one seminar. Each writing seminar has a
capacity of how many students can attend. There is an increasing penalty for
assigning students to their less preferred choice, e.g.

assignment    | penalty
--------------|--------
first choice  | 0
second choice | 1
third choice  | 5
fourth choice | 20
fifth choice  | 100
unassigned    | 1000

(Unassigned can mean either they are in a class they did not choose, or they are
not in any class). The goal is to find a feasible assignment that minimizes the
sum of the penalties over all students.

This method was used as of 2005 at Cornell to assign first year students to
freshman writing seminars.

We model the problem below with MIP, but it is in fact just a min-cost flow
problem. Solving the LP relaxation of this MIP with a simplex based solver
gives integer solutions.

Warning: this method is not strategy proof, students can get a better assignment
by not submitting their true preferences over the seminars.
"""

import dataclasses
import datetime
import random
from typing import Dict, Sequence, Tuple

from absl import app
from absl import flags

from ortools.math_opt.python import mathopt


_SOLVER_TYPE = flags.DEFINE_enum_class(
    "solver_type",
    mathopt.SolverType.CP_SAT,
    mathopt.SolverType,
    "The solver to use, must be an LP solver if lp_relax=True (e.g. GLOP) and"
    " must be an IP solver (e.g. SCIP, CP-SAT) otherwise.",
)

_NUM_CLASSES = flags.DEFINE_integer(
    "num_classes", 50, "How many random classes to create."
)

_LP_RELAX = flags.DEFINE_boolean(
    "lp_relax", False, "Solve the LP relaxation of the problem."
)

_TEST_DATA = flags.DEFINE_boolean(
    "test_data", False, "Use the small test instance."
)


@dataclasses.dataclass(frozen=True)
class Student:
  preferred_classes: Tuple[int, ...]
  name: str = ""


@dataclasses.dataclass(frozen=True)
class Seminar:
  capacity: int
  name: str = ""


@dataclasses.dataclass(frozen=True)
class WritingSeminarAssignmentProblem:
  seminars: Tuple[Seminar, ...]
  students: Tuple[Student, ...]
  rank_penalty: Tuple[float, ...]
  unmatched_penalty: float


def _test_problem() -> WritingSeminarAssignmentProblem:
  """A small deterministic instance for testing only."""
  return WritingSeminarAssignmentProblem(
      seminars=(Seminar(capacity=1, name="c1"), Seminar(capacity=1, name="c2")),
      students=(
          Student(preferred_classes=(1, 0), name="s1"),
          Student(preferred_classes=(0, 1), name="s2"),
      ),
      rank_penalty=(0, 10),
      unmatched_penalty=100,
  )


def _random_writing_seminar_assignment_problem(
    *,
    num_classes: int,
    class_capacity: int,
    num_students: int,
    selections_per_student: int,
    unmatched_penalty: float,
    rank_penalty: Tuple[float, ...],
) -> WritingSeminarAssignmentProblem:
  """Generates a random instance of the WritingSeminarAssignmentProblem."""
  if len(rank_penalty) != selections_per_student:
    raise ValueError(
        f"len(rank_penalty): {len(rank_penalty)} must equal"
        f" selections_per_student: {selections_per_student}"
    )
  seminars = tuple(
      Seminar(capacity=class_capacity, name=f"c_{i}")
      for i in range(num_classes)
  )
  students = []
  all_class_ids = list(range(num_classes))
  for s in range(num_students):
    preferred = tuple(random.sample(all_class_ids, selections_per_student))
    students.append(Student(preferred_classes=preferred, name=f"s_{s}"))
  return WritingSeminarAssignmentProblem(
      seminars=seminars,
      students=tuple(students),
      rank_penalty=rank_penalty,
      unmatched_penalty=unmatched_penalty,
  )


def _assign_students(
    problem: WritingSeminarAssignmentProblem,
    solver_type: mathopt.SolverType,
    time_limit: datetime.timedelta,
) -> Dict[Student, Seminar]:
  """Optimally assigns students to classes by solving an IP."""
  # Problem data:
  #   * i in S: the students.
  #   * j in C: the classes (writing seminars).
  #   * c_j: the capacity of class j.
  #   * K: how many classes each student ranks.
  #   * R_i for i in S, an ordered list K classes ranked for student i.
  #   * p_k for k = 1,...,K the penalty for giving a student their kth choice.
  #   * P: the penalty for not assigning a student.
  #
  # Decision variables:
  #   x_i_j: student i takes seminar j
  #   y_i: student i is not assigned
  #
  # Problem formulation:
  #   min     sum_i P y_i +  sum_{k, j in enumerate(R_i)} p_k x_i_j
  #   s.t.    y_i + sum_{j in R_i} x_i_j = 1    for all i in S
  #           sum_{i : j in R_i} x_i_j <= c_j   for all j in C
  #           x_i_j in {0, 1}                   for all i in S, for all j in R_i
  #           y_i in {0, 1}                     for all i in S
  model = mathopt.Model()
  use_int_vars = not _LP_RELAX.value
  # The y_i variables.
  unassigned = [
      model.add_variable(lb=0, ub=1, is_integer=use_int_vars, name=f"y_{i}")
      for i, _ in enumerate(problem.students)
  ]

  # The x_i_j variables.
  assignment_vars = [{} for _ in range(len(problem.students))]
  for i, student in enumerate(problem.students):
    for rank, j in enumerate(student.preferred_classes):
      assignment_vars[i][j] = model.add_variable(
          lb=0, ub=1, is_integer=use_int_vars, name=f"x_{i}_{j}"
      )
  # Transpose the variables in x. The first index of students_in_seminar
  # is the class (j). The value is an unordered list of the variables for
  # assigning students into this class.
  students_in_seminar = [[] for _ in range(len(problem.seminars))]
  for seminar_to_x in assignment_vars:
    for j, x in seminar_to_x.items():
      students_in_seminar[j].append(x)

  # Create the objective
  penalties = mathopt.fast_sum(unassigned) * problem.unmatched_penalty
  for i, student in enumerate(problem.students):
    penalties += mathopt.fast_sum(
        problem.rank_penalty[rank] * assignment_vars[i][j]
        for rank, j in enumerate(student.preferred_classes)
    )
  model.minimize(penalties)

  # Each student is in at most one class
  for i, student in enumerate(problem.students):
    model.add_linear_constraint(
        unassigned[i] + mathopt.fast_sum(assignment_vars[i].values()) == 1.0
    )

  # Each class does not exceed its capacity
  for j, seminar in enumerate(problem.seminars):
    model.add_linear_constraint(
        mathopt.fast_sum(students_in_seminar[j]) <= seminar.capacity
    )

  solve_result = mathopt.solve(
      model,
      solver_type,
      params=mathopt.SolveParameters(enable_output=True, time_limit=time_limit),
  )
  if solve_result.termination.reason not in {
      mathopt.TerminationReason.OPTIMAL,
      mathopt.TerminationReason.FEASIBLE,
  }:
    raise RuntimeError(
        f"failed to find a feasible solution: {solve_result.termination}"
    )

  assignment = {}
  for i, student in enumerate(problem.students):
    for sem, x in assignment_vars[i].items():
      x_val = solve_result.variable_values(x)
      int_err = min(abs(x_val), abs(1 - x_val))
      if int_err > 1e-3:
        raise RuntimeError(
            "all variables should be within 1e-3 of either 0 or 1, but found"
            f" value: {x_val}"
        )
      if solve_result.variable_values(x) > 0.5:
        assignment[student] = problem.seminars[sem]
  return assignment


def main(args: Sequence[str]) -> None:
  del args  # Unused.
  if _TEST_DATA.value:
    problem = _test_problem()
  else:
    num_classes = _NUM_CLASSES.value
    class_capacity = 15
    num_students = num_classes * 12
    selections_per_student = 5
    unmatched_penalty = 1000
    rank_penalty = (0, 1, 5, 20, 100)
    problem = _random_writing_seminar_assignment_problem(
        num_classes=num_classes,
        class_capacity=class_capacity,
        num_students=num_students,
        selections_per_student=selections_per_student,
        unmatched_penalty=unmatched_penalty,
        rank_penalty=rank_penalty,
    )
  assignment = _assign_students(
      problem, _SOLVER_TYPE.value, datetime.timedelta(minutes=1)
  )
  for student, seminar in assignment.items():
    print(f"{student.name}: {seminar.name}")
  num_unassigned = len(problem.students) - len(assignment)
  print(f"Unassigned students: {num_unassigned}")


if __name__ == "__main__":
  app.run(main)
