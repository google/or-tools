# Copyright 2010-2011 Google
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



from google.apputils import app
from graph import pywrapgraph


def RunAssignmentOn4x4Matrix(forward_graph):
  """Test linear sum assignment on a 4x4 matrix.

  Arguments:
    forward_graph: if true uses a ForwardStarGraph or a StarGraph to model
                   the assignment problem.
  """
  num_sources = 4
  num_targets = 4
  cost = [[90, 76, 75, 80],
          [35, 85, 55, 65],
          [125, 95, 90, 105],
          [45, 110, 95, 115]]
  expected_cost = cost[0][3] + cost[1][2] + cost[2][1] + cost[3][0]

  if forward_graph:
    graph = pywrapgraph.ForwardStarGraph(num_sources + num_targets,
                                         num_sources * num_targets)
    assignment = pywrapgraph.ForwardEbertLinearSumAssignment(graph, num_sources)
  else:
    graph = pywrapgraph.StarGraph(num_sources + num_targets,
                                  num_sources * num_targets)
    assignment = pywrapgraph.EbertLinearSumAssignment(graph, num_sources)

  for source in range (0, num_sources):
    for target in range(0, num_targets):
      arc = graph.AddArc(source, num_sources + target)
      assignment.SetArcCost(arc, cost[source][target])

  assignment.ComputeAssignment()
  total_cost = assignment.GetCost()
  print 'total cost', total_cost, '/', expected_cost


def main(unused_argv):
  RunAssignmentOn4x4Matrix(True)
  RunAssignmentOn4x4Matrix(False)

if __name__ == '__main__':
  app.run()
