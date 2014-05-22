# Various calls to CP api from python to verify they work.
from ortools.constraint_solver import pywrapcp
from ortools.constraint_solver import model_pb2
from ortools.constraint_solver import search_limit_pb2


def test_member():
  solver = pywrapcp.Solver('test member')
  x = solver.IntVar(1, 10, 'x')
  ct = x.Member([1, 2, 3, 5])
  print(ct)

def test_sparse_var():
  solver = pywrapcp.Solver('test sparse')
  x = solver.IntVar([1, 3, 5], 'x')
  print(x)

def test_modulo():
  solver = pywrapcp.Solver('test modulo')
  x = solver.IntVar(0, 10, 'x')
  y = solver.IntVar(2, 4, 'y')
  print(x % 3)
  print(x % y)

def test_limit():
  solver = pywrapcp.Solver('test limit')
  limit_proto = search_limit_pb2.SearchLimitProto()
  limit_proto.time = 10000
  limit_proto.branches = 10
  print limit_proto
  limit = solver.Limit(limit_proto)
  print limit

def test_export():
  solver = pywrapcp.Solver('test export')
  x = solver.IntVar(1, 10, 'x')
  ct = x.Member([1, 2, 3, 5])
  solver.Add(ct)
  proto = model_pb2.CPModelProto()
  proto.model = 'wrong name'
  solver.ExportModel(proto)
  print repr(proto)
  print str(proto)

class SearchMonitorTest(pywrapcp.SearchMonitor):
  def __init__(self, solver, nexts):
    pywrapcp.SearchMonitor.__init__(self, solver)
    self._nexts = nexts

  def BeginInitialPropagation(self):
    print self._nexts

  def EndInitialPropagation(self):
    print self._nexts


def test_search_monitor():
  solver = pywrapcp.Solver('test export')
  x = solver.IntVar(1, 10, 'x')
  ct = (x == 3)
  solver.Add(ct)
  db = solver.Phase([x], solver.CHOOSE_FIRST_UNBOUND, solver.ASSIGN_MIN_VALUE)
  monitor = SearchMonitorTest(solver, x)
  solver.Solve(db, monitor)


class DemonTest(pywrapcp.PyDemon):
  def __init__(self, x):
    pywrapcp.Demon.__init__(self)
    self._x = x
    print 'Demon built'

  def Run(self, solver):
    print 'in Run(), saw ' + str(self._x)


class ConstraintTest(pywrapcp.PyConstraint):
  def __init__(self, solver, x):
    pywrapcp.Constraint.__init__(self, solver)
    self._x = x
    print 'Constraint built'

  def Post(self):
    print 'in Post()'
    self._demon = DemonTest(self._x)
    self._x.WhenBound(self._demon)
    print 'out of Post()'

  def InitialPropagate(self):
    print 'in InitialPropagate()'
    self._x.SetMin(5)
    print self._x
    print 'out of InitialPropagate()'


def test_demon():
  solver = pywrapcp.Solver('test export')
  x = solver.IntVar(1, 10, 'x')
  demon = DemonTest(x)
  demon.Run(solver)


def test_constraint():
  solver = pywrapcp.Solver('test export')
  x = solver.IntVar(1, 10, 'x')
  myct = ConstraintTest(solver, x)
  solver.Add(myct)
  db = solver.Phase([x], solver.CHOOSE_FIRST_UNBOUND, solver.ASSIGN_MIN_VALUE)
  solver.Solve(db)


class InitialPropagateDemon(pywrapcp.PyDemon):
  def __init__(self, ct):
    pywrapcp.Demon.__init__(self)
    self._ct = ct

  def Run(self, solver):
    self._ct.InitialPropagate()


class DumbGreaterOrEqualToFive(pywrapcp.PyConstraint):
  def __init__(self, solver, x):
    pywrapcp.Constraint.__init__(self, solver)
    self._x = x

  def Post(self):
    self._demon = InitialPropagateDemon(self)
    self._x.WhenBound(self._demon)

  def InitialPropagate(self):
    if self._x.Bound():
      if self._x.Value() < 5:
        print 'Reject %d' % self._x.Value()
        self.solver().Fail()
      else:
        print 'Accept %d' % self._x.Value()


def test_failing_constraint():
  solver = pywrapcp.Solver('test export')
  x = solver.IntVar(1, 10, 'x')
  myct = DumbGreaterOrEqualToFive(solver, x)
  solver.Add(myct)
  db = solver.Phase([x], solver.CHOOSE_FIRST_UNBOUND, solver.ASSIGN_MIN_VALUE)
  solver.Solve(db)


def main():
  test_member()
  test_sparse_var()
  test_modulo()
  test_limit()
  test_export()
  test_search_monitor()
  test_demon()
  test_constraint()
  test_failing_constraint()


if __name__ == '__main__':
  main()
