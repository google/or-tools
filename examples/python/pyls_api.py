from ortools.constraint_solver import pywrapcp
from google.apputils import app
import gflags
import random

class OneVarLns(pywrapcp.PyLns):
  """One Var LNS."""

  def __init__(self, vars):
    pywrapcp.PyLns.__init__(self, vars)
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

  def __init__(self, vars):
    pywrapcp.IntVarLocalSearchOperator.__init__(self, vars)
    self.__index = 0
    self.__up = False

  def OneNeighbor(self):
    current_value = OldValue(self.__index)
    if self.__up:
      self.SetValue(self.__index, current_value + 1)
      self.__index = (self.__index + 1) % self.Size()
    else:
      SetValue(self.__index, current_value - 1)
    self.__up = not self.__up
    return True

  def OnStart(self):
    pass



def Solve(type):
  solver = pywrapcp.Solver('Solve')
  vars = [solver.IntVar(0, 4) for _ in range(4)]
  sum_var = solver.Sum(vars)
  obj = solver.Minimize(sum_var, 1)
  db = solver.Phase(vars, solver.CHOOSE_FIRST_UNBOUND, solver.ASSIGN_MAX_VALUE)
  ls = None

  if type == 0:  # LNS
    print 'Large Neighborhood Search'
    one_var_lns = OneVarLns(vars)
    ls_params = solver.LocalSearchPhaseParameters(one_var_lns, db)
    ls = solver.LocalSearchPhase(vars, db, ls_params)
  elif type == 1:  # LS
    print 'Local Search'
    move_one_var = MoveOneVar(vars)
    ls_params = solver.LocalSearchPhaseParameters(move_one_var, db)
    ls = solver.LocalSearchPhase(vars, db, ls_params)

  collector = solver.LastSolutionCollector()
  collector.Add(vars)
  collector.AddObjective(sum_var)
  log = solver.SearchLog(1000, obj)
  solver.Solve(ls, [collector, obj, log])
  print 'Objective value = %d' % collector.ObjectiveValue(0)


def main(_):
  Solve(0)
  Solve(1)


if __name__ == '__main__':
  app.run()
