from constraint_solver import pywrapcp

def test_member():
  solver = pywrapcp.Solver('test member')
  x = solver.IntVar(1, 10, 'x')
  ct = x.Member([1, 2, 3, 5])
  print ct


def main():
  test_member()


if __name__ == '__main__':
  main()
