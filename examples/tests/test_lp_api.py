from linear_solver import pywraplp
import types

def Sum(arg):
  if type(arg) is types.GeneratorType:
    arg = [x for x in arg]
  sum = 0;
  for i in arg:
    sum += i
  print("sum(%s) = %d" % (str(arg), sum))

def test_sum_no_brackets():
  Sum(x for x in range(10) if x % 2 == 0)
  Sum([x for x in range(10) if x % 2 == 0])


def main():
  test_sum_no_brackets()


if __name__ == '__main__':
  main()
