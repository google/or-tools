#!/usr/bin/env python3
from google.protobuf import text_format
from ortools.linear_solver import pywraplp
from ortools.linear_solver import linear_solver_pb2
import sys
import types


def Sum(arg):
  if type(arg) is types.GeneratorType:
    arg = [x for x in arg]
  sum = 0
  for i in arg:
    sum += i
  print('sum(%s) = %d' % (str(arg), sum))


def test_sum_no_brackets():
  Sum(x for x in range(10) if x % 2 == 0)
  Sum([x for x in range(10) if x % 2 == 0])

text_model = """
solver_type:CBC_MIXED_INTEGER_PROGRAMMING
model <
  maximize:true
  variable < lower_bound:1 upper_bound:10 objective_coefficient:2 >
  variable < lower_bound:1 upper_bound:10 objective_coefficient:1 >
  constraint < lower_bound:-10000 upper_bound:4
    var_index:0
    var_index:1
    coefficient:1
    coefficient:2
  >
>
"""


def test_proto():
  input_proto = linear_solver_pb2.MPModelRequest()
  text_format.Merge(text_model, input_proto)
  solver = pywraplp.Solver('solveFromProto',
                           pywraplp.Solver.CBC_MIXED_INTEGER_PROGRAMMING)
  print(input_proto)
  # For now, create the model from the proto by parsing the proto
  solver.LoadModelFromProto(input_proto.model)
  solver.EnableOutput()
  solver.Solve()
  # Fill solution
  solution = linear_solver_pb2.MPSolutionResponse()
  solver.FillSolutionResponseProto(solution)
  print(solution)


def main():
  test_sum_no_brackets()
  #  test_proto()


if __name__ == '__main__':
  main()
