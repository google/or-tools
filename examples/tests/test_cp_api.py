# Various calls to CP api from python to verify they work.
from constraint_solver import pywrapcp


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


def main():
  test_member()
  test_sparse_var()
  test_modulo()


if __name__ == '__main__':
  main()
