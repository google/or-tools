from ortools.constraint_solver import pywrapcp


class OneVarLns(pywrapcp.BaseLns):
  """One Var LNS."""

  def __init__(self, vars):
    pywrapcp.BaseLns.__init__(self, vars)
    self.__index = 0

  def InitFragments(self):
    self.__index = 0

  def NextFragment(self):
    if self.__index < self.Size():
      self.AppendToFragment(self.__index)
      self.__index += 1
      return True
    else:
      return False


class MoveOneVar(pywrapcp.IntVarLocalSearchOperator):
  """Move one var up or down."""

  def __init__(self, vars):
    pywrapcp.IntVarLocalSearchOperator.__init__(self, vars)
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

  def __init__(self, vars):
    pywrapcp.IntVarLocalSearchFilter.__init__(self, vars)
    self.__sum = 0

  def OnSynchronize(self, delta):
    self.__sum = sum(self.Value(index) for index in range(self.Size()))

  def Accept(self, delta, unused_delta_delta, unused_objective_min,
             unused_objective_max):
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


def Solve(type):
  solver = pywrapcp.Solver('Solve')
  vars = [solver.IntVar(0, 4) for _ in range(4)]
  sum_var = solver.Sum(vars).Var()
  obj = solver.Minimize(sum_var, 1)
  db = solver.Phase(vars, solver.CHOOSE_FIRST_UNBOUND, solver.ASSIGN_MAX_VALUE)
  ls = None

  if type == 0:  # LNS
    print('Large Neighborhood Search')
    one_var_lns = OneVarLns(vars)
    ls_params = solver.LocalSearchPhaseParameters(sum_var, one_var_lns, db)
    ls = solver.LocalSearchPhase(vars, db, ls_params)
  elif type == 1:  # LS
    print('Local Search')
    move_one_var = MoveOneVar(vars)
    ls_params = solver.LocalSearchPhaseParameters(sum_var, move_one_var, db)
    ls = solver.LocalSearchPhase(vars, db, ls_params)
  else:
    print('Local Search with Filter')
    move_one_var = MoveOneVar(vars)
    sum_filter = SumFilter(vars)
    filter_manager = pywrapcp.LocalSearchFilterManager([sum_filter])
    ls_params = solver.LocalSearchPhaseParameters(sum_var, move_one_var, db, None,
                                                  filter_manager)
    ls = solver.LocalSearchPhase(vars, db, ls_params)

  collector = solver.LastSolutionCollector()
  collector.Add(vars)
  collector.AddObjective(sum_var)
  log = solver.SearchLog(1000, obj)
  solver.Solve(ls, [collector, obj, log])
  print('Objective value = %d' % collector.ObjectiveValue(0))


def main():
  Solve(0)
  Solve(1)
  Solve(2)


if __name__ == '__main__':
  main()
