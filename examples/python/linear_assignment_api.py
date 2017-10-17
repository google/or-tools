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

"""Test linear sum assignment on a 4x4 matrix.

   Example taken from:
   http://www.ee.oulu.fi/~mpa/matreng/eem1_2-1.htm with kCost[0][1]
   modified so the optimum solution is unique.
"""
from __future__ import print_function


from ortools.graph import pywrapgraph


def RunAssignmentOn4x4Matrix():
  """Test linear sum assignment on a 4x4 matrix.
  """
  num_sources = 4
  num_targets = 4
  cost = [[90, 76, 75, 80],
          [35, 85, 55, 65],
          [125, 95, 90, 105],
          [45, 110, 95, 115]]
  expected_cost = cost[0][3] + cost[1][2] + cost[2][1] + cost[3][0]

  assignment = pywrapgraph.LinearSumAssignment()
  for source in range(0, num_sources):
    for target in range(0, num_targets):
      assignment.AddArcWithCost(source, target, cost[source][target])

  solve_status = assignment.Solve()
  if solve_status == assignment.OPTIMAL:
    print('Successful solve.')
    print('Total cost', assignment.OptimalCost(), '/', expected_cost)
    for i in range(0, assignment.NumNodes()):
      print('Left node %d assigned to right node %d with cost %d.' % (
          i,
          assignment.RightMate(i),
          assignment.AssignmentCost(i)))
  elif solve_status == assignment.INFEASIBLE:
    print('No perfect matching exists.')
  elif solve_status == assignment.POSSIBLE_OVERFLOW:
    print('Some input costs are too large and may cause an integer overflow.')


def main():
  RunAssignmentOn4x4Matrix()

if __name__ == '__main__':
  main()
