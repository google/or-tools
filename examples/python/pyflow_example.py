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

"""MaxFlow and MinCostFlow examples."""


from __future__ import print_function
from ortools.graph import pywrapgraph


def MaxFlow():
  """MaxFlow simple interface example."""
  print('MaxFlow on a simple network.')
  tails = [0, 0, 0, 0, 1, 2, 3, 3, 4]
  heads = [1, 2, 3, 4, 3, 4, 4, 5, 5]
  capacities = [5, 8, 5, 3, 4, 5, 6, 6, 4]
  expected_total_flow = 10
  max_flow = pywrapgraph.SimpleMaxFlow()
  for i in range(0, len(tails)):
    max_flow.AddArcWithCapacity(tails[i], heads[i], capacities[i])
  if max_flow.Solve(0, 5) == max_flow.OPTIMAL:
    print('Total flow', max_flow.OptimalFlow(), '/', expected_total_flow)
    for i in range(max_flow.NumArcs()):
      print(('From source %d to target %d: %d / %d' % (
          max_flow.Tail(i),
          max_flow.Head(i),
          max_flow.Flow(i),
          max_flow.Capacity(i))))
    print('Source side min-cut:', max_flow.GetSourceSideMinCut())
    print('Sink side min-cut:', max_flow.GetSinkSideMinCut())
  else:
    print('There was an issue with the max flow input.')


def MinCostFlow():
  """MinCostFlow simple interface example.

  Note that this example is actually a linear sum assignment example and will
  be more efficiently solved with the pywrapgraph.LinearSumAssignement class.
  """
  print('MinCostFlow on 4x4 matrix.')
  num_sources = 4
  num_targets = 4
  costs = [[90, 75, 75, 80],
           [35, 85, 55, 65],
           [125, 95, 90, 105],
           [45, 110, 95, 115]]
  expected_cost = 275
  min_cost_flow = pywrapgraph.SimpleMinCostFlow()
  for source in range(0, num_sources):
    for target in range(0, num_targets):
      min_cost_flow.AddArcWithCapacityAndUnitCost(
          source, num_sources + target, 1, costs[source][target])
  for node in range(0, num_sources):
    min_cost_flow.SetNodeSupply(node, 1)
    min_cost_flow.SetNodeSupply(num_sources + node, -1)
  status = min_cost_flow.Solve()
  if status == min_cost_flow.OPTIMAL:
    print('Total flow', min_cost_flow.OptimalCost(), '/', expected_cost)
    for i in range(0, min_cost_flow.NumArcs()):
      if min_cost_flow.Flow(i) > 0:
        print('From source %d to target %d: cost %d' % (
            min_cost_flow.Tail(i),
            min_cost_flow.Head(i) - num_sources,
            min_cost_flow.UnitCost(i)))
  else:
    print('There was an issue with the min cost flow input.')


def main():
  MaxFlow()
  MinCostFlow()


if __name__ == '__main__':
  main()
