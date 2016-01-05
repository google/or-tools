from ortools.constraint_solver import pywrapcp


small = [[3, 2, -1, 3], [-1, -1, -1, 2], [3, -1, -1, -1], [3, -1, 3, 1]]


def NeighboringArcs(i, j, h_arcs, v_arcs):
  tmp = []
  if j > 0:
    tmp.append(h_arcs[i][j - 1])
  if j < len(v_arcs) - 1:
    tmp.append(h_arcs[i][j])
  if i > 0:
    tmp.append(v_arcs[j][i - 1])
  if i < len(h_arcs) - 1:
    tmp.append(v_arcs[j][i])
  return tmp


class BooleanSumEvenUpdateDemon(pywrapcp.PyDemon):
  def __init__(self, ct, index):
    pywrapcp.PyDemon.__init__(self)
    self.__ct = ct
    self.__index = index

  def Run(self, solver):
    self.__ct.Update(self.__index)

  def DebugString(self):
    return 'BooleanSumEvenUpdateDemon(%i)' % self.__index


class BooleanSumEven(pywrapcp.PyConstraint):

  def __init__(self, solver, vars):
    pywrapcp.PyConstraint.__init__(self, solver)
    self.__vars = vars
    self.__num_possible_true_vars = pywrapcp.RevInteger(0)
    self.__num_always_true_vars = pywrapcp.RevInteger(0)

  def Post(self):
    for i in range(len(self.__vars)):
      v = self.__vars[i]
      if not v.Bound():
        demon = self.Demon(BooleanSumEven.Update, i)
        v.WhenBound(demon)

  def InitialPropagate(self):
    num_always_true = 0
    num_possible_true = 0
    possible_true_index = -1
    for i in range(len(self.__vars)):
      var = self.__vars[i]
      if var.Min() == 1:
        num_always_true += 1
        num_possible_true += 1
      elif var.Max() == 1:
        num_possible_true += 1
        possible_true_index = i

    if num_always_true == num_possible_true and num_possible_true % 2 == 1:
      self.solver().Fail()

    if num_possible_true == num_always_true + 1:
      self.__vars[possible_true_index].SetValue(num_always_true % 2)

    self.__num_possible_true_vars.SetValue(self.solver(), num_possible_true)
    self.__num_always_true_vars.SetValue(self.solver(), num_always_true)

  def Update(self, index):
    solver = self.solver()
    value = self.__vars[index].Value()
    if value == 0:
      self.__num_possible_true_vars.SetValue(solver, self.__num_possible_true_vars.Value() - 1)
    else:
      self.__num_always_true_vars.SetValue(solver, self.__num_always_true_vars.Value() + 1)

    num_possible = self.__num_possible_true_vars.Value()
    num_always = self.__num_always_true_vars.Value()

    if num_always == num_possible and num_possible % 2 == 1:
      solver.Fail()

    if num_possible == num_always + 1:
      possible_true_index = -1
      for i in range(len(self.__vars)):
        if not self.__vars[i].Bound():
          possible_true_index = i
          break

      if possible_true_index != -1:
        self.__vars[possible_true_index].SetValue(num_always % 2)

  def DebugString(self):
    return 'BooleanSumEven'


def SlitherLink(data):
  num_rows = len(data)
  num_columns = len(data[0])

  solver = pywrapcp.Solver('slitherlink')
  h_arcs = [[solver.BoolVar('h_arcs[%i][%i]' % (i, j))
             for j in range(num_columns)]
            for i in range(num_rows + 1)]

  v_arcs = [[solver.BoolVar('v_arcs[%i][%i]' % (i, j))
             for j in range(num_rows)]
            for i in range(num_columns + 1)]

  for i in range(num_rows):
    for j in range(num_columns):
      if data[i][j] != -1:
        sq = [h_arcs[i][j], h_arcs[i + 1][j], v_arcs[j][i], v_arcs[j + 1][i]]
        solver.Add(solver.SumEquality(sq, data[i][j]))

  zero_or_two = [0, 2]
  for i in range(num_rows + 1):
    for j in range(num_columns + 1):
      neighbors = NeighboringArcs(i, j, h_arcs, v_arcs)
      solver.Add(solver.Sum(neighbors).Member(zero_or_two))

  for i in range(num_columns):
    column = [h_arcs[j][i] for j in range(num_rows + 1)]
    solver.Add(BooleanSumEven(solver, column))

  for i in range(num_rows):
    row = [v_arcs[j][i] for j in range(num_columns + 1)]
    solver.Add(BooleanSumEven(solver, row))

  all_vars = []
  for row in h_arcs:
    all_vars.extend(row)
  for column in v_arcs:
    all_vars.extend(column)

  db = solver.Phase(all_vars,
                    solver.CHOOSE_FIRST_UNBOUND,
                    solver.ASSIGN_MAX_VALUE)

  log = solver.SearchLog(1000000)

  solver.NewSearch(db, log)
  while solver.NextSolution():
    print 'Solution'

  solver.EndSearch()


if __name__ == '__main__':
  SlitherLink(small)
