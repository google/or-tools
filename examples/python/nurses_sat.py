# Copyright 2010-2017 Google
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

from __future__ import print_function
import sys
from ortools.sat.python import cp_model


class NursesPartialSolutionPrinter(cp_model.CpSolverSolutionCallback):
  """Print intermediate solutions."""

  def __init__(self, shifts, num_nurses, num_days, num_shifts, sols):
    self.__shifts = shifts
    self.__num_nurses = num_nurses
    self.__num_days = num_days
    self.__num_shifts = num_shifts
    self.__solutions = set(sols)
    self.__solution_count = 0

  def NewSolution(self):
    self.__solution_count += 1
    if self.__solution_count in self.__solutions:
      print('Solution #%i' % self.__solution_count)
      for d in range(self.__num_days):
        print('Day #%i' % d)
        for n in range(self.__num_nurses):
          for s in range(self.__num_shifts):
            if self.Value(self.__shifts[(n, d, s)]):
              print('  Nurse #%i is working shift #%i' % (n, s))
      print()

  def SolutionCount(self):
    return self.__solution_count


def main():
  # Data.
  num_nurses = 4
  num_shifts = 4  # Nurse assigned to shift 0 means not working that day.
  num_days = 7
  all_nurses = range(num_nurses)
  all_shifts = range(num_shifts)
  all_working_shifts = range(1, num_shifts)
  all_days = range(num_days)

  # Creates the model.
  model = cp_model.CpModel()

  # Creates shift variables.
  # shifts[(n, d, s)]: nurse 'n' works shift 's' on day 'd'.
  shifts = {}
  for n in all_nurses:
    for d in all_days:
      for s in all_shifts:
        shifts[(n, d, s)] = model.NewBoolVar('shift_n%id%is%i' % (n, d, s))

  # Makes assignments different on each day, that is each shift is assigned at
  # most one nurse. As we have the same number of nurses and shifts, then each
  # day, each shift is assigned to exactly one nurse.
  for d in all_days:
    for s in all_shifts:
      model.Add(sum(shifts[(n, d, s)] for n in all_nurses) == 1)

  # Nurses do 1 shift per day.
  for n in all_nurses:
    for d in all_days:
      model.Add(sum(shifts[(n, d, s)] for s in all_shifts) == 1)

  # Each nurse works 5 or 6 days in a week.
  # That is each nurse works shift 0 at most 2 times.
  for n in all_nurses:
    model.AddSumConstraint([shifts[(n, d, 0)] for d in all_days], 1, 2)

  # works_shift[(n, s)] is 1 if nurse n works shift s at least one day in
  # the week.
  works_shift = {}
  for n in all_nurses:
    for s in all_shifts:
      works_shift[(n, s)] = model.NewBoolVar('works_shift_n%is%i' % (n, s))
      model.AddMaxEquality(works_shift[(n, s)],
                           [shifts[(n, d, s)] for d in all_days])

  # For each shift, at most 2 nurses are assigned to that shift during the week.
  for s in all_working_shifts:
    model.Add(sum(works_shift[(n, s)] for n in all_nurses) <= 2)

  # If a nurse works shifts 2 or 3 on, she must also work that shift the
  # previous day or the following day.
  # This means that on a given day and shift, either she does not work that
  # shift on that day, or she works that shift on the day before, or the day
  # after.
  for n in all_nurses:
    for s in [2, 3]:
      for d in all_days:
        yesterday = (d - 1) % num_days
        tomorrow = (d + 1) % num_days
        model.AddBoolOr([
            shifts[(n, yesterday, s)], shifts[(n, d, s)].Not(),
            shifts[(n, tomorrow, s)]
        ])

  # Creates the solver and solve.
  solver = cp_model.CpSolver()
  # Display a few solutions picked at random.
  a_few_solutions = [859, 2034, 5091, 7003]
  solution_printer = NursesPartialSolutionPrinter(shifts, num_nurses, num_days,
                                                  num_shifts, a_few_solutions)
  status = solver.SearchForAllSolutions(model, solution_printer)

  # Statistics.
  print()
  print('Statistics')
  print('  - conflicts       : %i' % solver.NumConflicts())
  print('  - branches        : %i' % solver.NumBranches())
  print('  - wall time       : %f ms' % solver.WallTime())
  print('  - solutions found : %i' % solution_printer.SolutionCount())


if __name__ == '__main__':
  main()
