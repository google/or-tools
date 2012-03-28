# Copyright 2010-2012 Google
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

"""Min Cost Flow and Max Flow examples."""



from google.apputils import app
import gflags
from graph import pywrapgraph

FLAGS = gflags.FLAGS


def MinCostFlow():
  """Min Cost Flow Example."""
  print 'Min Cost Flow on 4x4 Matrix'
  num_sources = 4
  num_targets = 4
  costs = [[90, 75, 75, 80],
           [35, 85, 55, 65],
           [125, 95, 90, 105],
           [45, 110, 95, 115]]
  expected_cost = 275
  graph = pywrapgraph.StarGraph(num_sources + num_targets,
                                num_sources * num_targets)
  min_cost_flow = pywrapgraph.MinCostFlow(graph)
  for source in range(0, num_sources):
    for target in range(0, num_targets):
      arc = graph.AddArc(source, num_sources + target)
      min_cost_flow.SetArcUnitCost(arc, costs[source][target])
      min_cost_flow.SetArcCapacity(arc, 1)

  for source in range(0, num_sources):
    min_cost_flow.SetNodeSupply(source, 1)
  for target in range(0, num_targets):
    min_cost_flow.SetNodeSupply(num_sources + target, -1)
  min_cost_flow.Solve()
  total_flow_cost = min_cost_flow.GetOptimalCost()
  print 'total flow', total_flow_cost, '/', expected_cost


def MaxFeasibleFlow():
  """Max Flow Example."""
  print 'MaxFeasibleFlow'
  num_nodes = 6
  num_arcs = 9
  tails = [0, 0, 0, 0, 1, 2, 3, 3, 4]
  heads = [1, 2, 3, 4, 3, 4, 4, 5, 5]
  capacities = [5, 8, 5, 3, 4, 5, 6, 6, 4]
  expected_flows = [4, 4, 2, 0, 4, 4, 0, 6, 4]
  expected_total_flow = 10
  graph = pywrapgraph.StarGraph(num_nodes, num_arcs)
  max_flow = pywrapgraph.MaxFlow(graph, 0, num_nodes - 1)
  for i in range(0, num_arcs):
    arc = graph.AddArc(tails[i], heads[i])
    max_flow.SetArcCapacity(arc, capacities[i])
  max_flow.Solve()
  total_flow = max_flow.GetOptimalFlow()
  print 'total flow', total_flow, '/', expected_total_flow
  for i in range(num_arcs):
    print 'Arc', i, ':', max_flow.Flow(i), '/', expected_flows[i]


def main(unused_argv):
  MinCostFlow()
  MaxFeasibleFlow()


if __name__ == '__main__':
  app.run()
