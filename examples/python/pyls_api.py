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
    print 'OneVarLns::InitFragments'

  def NextFragment(self):
    if self.__index < self.Size():
      self.__index += 1
      return [self.__index - 1]
    else:
      return []


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

  collector = solver.LastSolutionCollector()
  collector.Add(vars)
  collector.AddObjective(sum_var)
  log = solver.SearchLog(1000, obj)
  solver.Solve(ls, [collector, obj, log])
  print 'Objective value = %d' % collector.ObjectiveValue()


def main(_):
  Solve(0)


if __name__ == '__main__':
  app.run()
