# Copyright 2010-2014 Google
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
"""Examples showing how to use the python API of the constraint solver.

See python/constraint_solver.swig.
This example file can also be used as a unit test.
"""

from ortools.constraint_solver import pywrapcp

from google.apputils import app


class OneVarLns(pywrapcp.PyLns):
  """One Var LNS."""

  def __init__(self, variables):
    pywrapcp.PyLns.__init__(self, variables)
    self.__index = 0

  def InitFragments(self):
    self.__index = 0

  def NextFragment(self):
    if self.__index < self.Size():
      self.__index += 1
      return [self.__index - 1]
    else:
      return []


class MoveOneVar(pywrapcp.IntVarLocalSearchOperator):
  """Move one var up or down."""

  def __init__(self, variables):
    pywrapcp.IntVarLocalSearchOperator.__init__(self, variables)
    self.__index = 0
    self.__up = False

  def OneNeighbor(self):
    current_value = self.OldValue(self.__index)
    if self.__up:
      self.SetValue(self.__index, current_value + 1)
      self.__index = (self.__index + 1) % self.Size()
    else:
      self.SetValue(self.__index, current_value - 1)
    self.__up = not self.__up
    return True

  def OnStart(self):
    pass

  def IsIncremental(self):
    return False


class SumFilter(pywrapcp.IntVarLocalSearchFilter):
  """Filter to speed up LS computation."""

  def __init__(self, variables):
    pywrapcp.IntVarLocalSearchFilter.__init__(self, variables)
    self.__sum = 0

  def OnSynchronize(self, delta):
    self.__sum = sum(self.Value(index) for index in range(self.Size()))

  def Accept(self, delta, _):
    solution_delta = delta.IntVarContainer()
    solution_delta_size = solution_delta.Size()
    for i in range(solution_delta_size):
      if not solution_delta.Element(i).Activated():
        return True

    new_sum = self.__sum
    for i in range(solution_delta_size):
      element = solution_delta.Element(i)
      int_var = element.Var()
      touched_var_index = self.IndexFromVar(int_var)
      old_value = self.Value(touched_var_index)
      new_value = element.Value()
      new_sum += new_value - old_value

    return new_sum < self.__sum

  def IsIncremental(self):
    return False


def Solve(example_index):
  """Run one of 3 simple examples of local search."""
  solver = pywrapcp.Solver('Solve')
  variables = [solver.IntVar(0, 4) for _ in range(4)]
  sum_var = solver.Sum(variables)
  obj = solver.Minimize(sum_var, 1)
  db = solver.Phase(variables, solver.CHOOSE_FIRST_UNBOUND,
                    solver.ASSIGN_MAX_VALUE)
  ls = None

  if example_index == 0:  # LNS
    print 'Large Neighborhood Search'
    one_var_lns = OneVarLns(variables)
    ls_params = solver.LocalSearchPhaseParameters(one_var_lns, db)
    ls = solver.LocalSearchPhase(variables, db, ls_params)
  elif example_index == 1:  # LS
    print 'Local Search'
    move_one_var = MoveOneVar(variables)
    ls_params = solver.LocalSearchPhaseParameters(move_one_var, db)
    ls = solver.LocalSearchPhase(variables, db, ls_params)
  else:
    print 'Local Search with Filter'
    move_one_var = MoveOneVar(variables)
    sum_filter = SumFilter(variables)
    ls_params = solver.LocalSearchPhaseParameters(move_one_var, db, None,
                                                  [sum_filter])
    ls = solver.LocalSearchPhase(variables, db, ls_params)

  collector = solver.LastSolutionCollector()
  collector.Add(variables)
  collector.AddObjective(sum_var)
  log = solver.SearchLog(1000, obj)
  solver.Solve(ls, [collector, obj, log])
  print 'Objective value = %d' % collector.ObjectiveValue(0)


def main(_):
  Solve(0)
  Solve(1)
  Solve(2)


if __name__ == '__main__':
  app.run()
