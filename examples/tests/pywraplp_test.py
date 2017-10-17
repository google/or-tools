# Copyright 2010-2017 Google
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
"""Simple unit tests for python/linear_solver.swig. Not exhaustive."""

import types

from ortools.linear_solver import linear_solver_pb2
from ortools.linear_solver import pywraplp

from google.protobuf import text_format


def compute_sum(arg):
  if type(arg) is types.GeneratorType:
    arg = [x for x in arg]
  s = 0
  for i in arg:
    s += i
  print 'sum(%s) = %d' % (str(arg), s)


def test_sum_no_brackets():
  compute_sum(x for x in range(10) if x % 2 == 0)
  compute_sum([x for x in range(10) if x % 2 == 0])

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
  print input_proto
  # For now, create the model from the proto by parsing the proto
  errors = solver.LoadModelFromProto(input_proto.model)
  assert not errors
  solver.EnableOutput()
  solver.Solve()
  # Fill solution
  solution = linear_solver_pb2.MPSolutionResponse()
  solver.FillSolutionResponseProto(solution)
  print solution


def test_external_api():
  solver = pywraplp.Solver('TestExternalAPI',
                           pywraplp.Solver.GLOP_LINEAR_PROGRAMMING)
  infinity = solver.Infinity()
  infinity2 = solver.infinity()
  assert infinity == infinity2
  # x1, x2 and x3 are continuous non-negative variables.
  x1 = solver.NumVar(0.0, infinity, 'x1')
  x2 = solver.NumVar(0.0, infinity, 'x2')
  x3 = solver.NumVar(0.0, infinity, 'x3')
  assert x1.Lb() == 0
  assert x1.Ub() == infinity
  assert not x1.Integer()
  solver.Maximize(10 * x1 + 6 * x2 + 4 * x3 + 5)
  assert solver.Objective().Offset() == 5
  c0 = solver.Add(10 * x1 + 4 * x2 + 5 * x3 <= 600, 'ConstraintName0')
  c1 = solver.Add(2 * x1 + 2 * x2 + 6 * x3 <= 300)
  sum_of_vars = sum([x1, x2, x3])
  solver.Add(sum_of_vars <= 100.0, 'OtherConstraintName')
  assert c1.Lb() == -infinity
  assert c1.Ub() == 300
  c1.SetLb(-100000)
  assert c1.Lb() == -100000
  c1.SetUb(301)
  assert c1.Ub() == 301

  solver.SetTimeLimit(10000)
  result_status = solver.Solve()

  # The problem has an optimal solution.
  assert result_status == pywraplp.Solver.OPTIMAL
  print 'Problem solved in %f milliseconds' % solver.WallTime()
  print 'Problem solved in %d iterations' % solver.Iterations()
  print x1.ReducedCost()
  print c0.DualValue()


def main():
  test_sum_no_brackets()
  # TODO(user): Support the proto API in or-tools. When this happens, re-enable
  # the test below:
  # test_proto()
  test_external_api()


if __name__ == '__main__':
  main()
