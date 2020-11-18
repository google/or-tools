# Copyright 2010 Hakan Kjellerstrand hakank@gmail.com
#
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
"""

  3 jugs problem using regular constraint in Google CP Solver.

  A.k.a. water jugs problem.

  Problem from Taha 'Introduction to Operations Research',
  page 245f .

  For more info about the problem, see:
  http://mathworld.wolfram.com/ThreeJugProblem.html

  This model use a regular constraint for handling the
  transitions between the states. Instead of minimizing
  the cost in a cost matrix (as shortest path problem),
  we here call the model with increasing length of the
  sequence array (x).

  Compare with other models that use MIP/CP approach,
  as a shortest path problem:
  * Comet: http://www.hakank.org/comet/3_jugs.co
  * Comet: http://www.hakank.org/comet/water_buckets1.co
  * MiniZinc: http://www.hakank.org/minizinc/3_jugs.mzn
  * MiniZinc: http://www.hakank.org/minizinc/3_jugs2.mzn
  * SICStus: http://www.hakank.org/sicstus/3_jugs.pl
  * ECLiPSe: http://www.hakank.org/eclipse/3_jugs.ecl
  * ECLiPSe: http://www.hakank.org/eclipse/3_jugs2.ecl
  * Gecode: http://www.hakank.org/gecode/3_jugs2.cpp


  This model was created by Hakan Kjellerstrand (hakank@gmail.com)
  Also see my other Google CP Solver models:
  http://www.hakank.org/google_or_tools/

"""

from ortools.constraint_solver import pywrapcp
from collections import defaultdict

#
# Global constraint regular
#
# This is a translation of MiniZinc's regular constraint (defined in
# lib/zinc/globals.mzn), via the Comet code refered above.
# All comments are from the MiniZinc code.
# '''
# The sequence of values in array 'x' (which must all be in the range 1..S)
# is accepted by the DFA of 'Q' states with input 1..S and transition
# function 'd' (which maps (1..Q, 1..S) -> 0..Q)) and initial state 'q0'
# (which must be in 1..Q) and accepting states 'F' (which all must be in
# 1..Q).  We reserve state 0 to be an always failing state.
# '''
#
# x : IntVar array
# Q : number of states
# S : input_max
# d : transition matrix
# q0: initial state
# F : accepting states


def regular(x, Q, S, d, q0, F):

  solver = x[0].solver()

  assert Q > 0, 'regular: "Q" must be greater than zero'
  assert S > 0, 'regular: "S" must be greater than zero'

  # d2 is the same as d, except we add one extra transition for
  # each possible input;  each extra transition is from state zero
  # to state zero.  This allows us to continue even if we hit a
  # non-accepted input.

  # Comet: int d2[0..Q, 1..S]
  d2 = []
  for i in range(Q + 1):
    row = []
    for j in range(S):
      if i == 0:
        row.append(0)
      else:
        row.append(d[i - 1][j])
    d2.append(row)

  d2_flatten = [d2[i][j] for i in range(Q + 1) for j in range(S)]

  # If x has index set m..n, then a[m-1] holds the initial state
  # (q0), and a[i+1] holds the state we're in after processing
  # x[i].  If a[n] is in F, then we succeed (ie. accept the
  # string).
  x_range = list(range(0, len(x)))
  m = 0
  n = len(x)

  a = [solver.IntVar(0, Q + 1, 'a[%i]' % i) for i in range(m, n + 1)]

  # Check that the final state is in F
  solver.Add(solver.MemberCt(a[-1], F))
  # First state is q0
  solver.Add(a[m] == q0)
  for i in x_range:
    solver.Add(x[i] >= 1)
    solver.Add(x[i] <= S)

    # Determine a[i+1]: a[i+1] == d2[a[i], x[i]]
    solver.Add(
        a[i + 1] == solver.Element(d2_flatten, ((a[i]) * S) + (x[i] - 1)))


def main(n):

  # Create the solver.
  solver = pywrapcp.Solver('3 jugs problem using regular constraint')

  #
  # data
  #

  # the DFA (for regular)
  n_states = 14
  input_max = 15
  initial_state = 1  # 0 is for the failing state
  accepting_states = [15]

  ##
  # Manually crafted DFA
  # (from the adjacency matrix used in the other models)
  ##
  # transition_fn =  [
  #    # 1  2  3  4  5  6  7  8  9  0  1  2  3  4  5
  #     [0, 2, 0, 0, 0, 0, 0, 0, 9, 0, 0, 0, 0, 0, 0],  # 1
  #     [0, 0, 3, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0],  # 2
  #     [0, 0, 0, 4, 0, 0, 0, 0, 9, 0, 0, 0, 0, 0, 0],  # 3
  #     [0, 0, 0, 0, 5, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0],  # 4
  #     [0, 0, 0, 0, 0, 6, 0, 0, 9, 0, 0, 0, 0, 0, 0],  # 5
  #     [0, 0, 0, 0, 0, 0, 7, 0, 0, 0, 0, 0, 0, 0, 0],  # 6
  #     [0, 0, 0, 0, 0, 0, 0, 8, 9, 0, 0, 0, 0, 0, 0],  # 7
  #     [0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 15], # 8
  #     [0, 0, 0, 0, 0, 0, 0, 0, 0, 10, 0, 0, 0, 0, 0], # 9
  #     [0, 2, 0, 0, 0, 0, 0, 0, 0, 0, 11, 0, 0, 0, 0], # 10
  #     [0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 12, 0, 0, 0], # 11
  #     [0, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 13, 0, 0], # 12
  #     [0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 14, 0], # 13
  #     [0, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 15], # 14
  #                                                     # 15
  #     ]

  #
  # However, the DFA is easy to create from adjacency lists.
  #
  states = [
      [2, 9],  # state 1
      [3],  # state 2
      [4, 9],  # state 3
      [5],  # state 4
      [6, 9],  # state 5
      [7],  # state 6
      [8, 9],  # state 7
      [15],  # state 8
      [10],  # state 9
      [11],  # state 10
      [12],  # state 11
      [13],  # state 12
      [14],  # state 13
      [15]  # state 14
  ]

  transition_fn = []
  for i in range(n_states):
    row = []
    for j in range(1, input_max + 1):
      if j in states[i]:
        row.append(j)
      else:
        row.append(0)
    transition_fn.append(row)

  #
  # The name of the nodes, for printing
  # the solution.
  #
  nodes = [
      '8,0,0',  # 1 start
      '5,0,3',  # 2
      '5,3,0',  # 3
      '2,3,3',  # 4
      '2,5,1',  # 5
      '7,0,1',  # 6
      '7,1,0',  # 7
      '4,1,3',  # 8
      '3,5,0',  # 9
      '3,2,3',  # 10
      '6,2,0',  # 11
      '6,0,2',  # 12
      '1,5,2',  # 13
      '1,4,3',  # 14
      '4,4,0'  # 15 goal
  ]

  #
  # declare variables
  #
  x = [solver.IntVar(1, input_max, 'x[%i]' % i) for i in range(n)]

  #
  # constraints
  #
  regular(x, n_states, input_max, transition_fn, initial_state,
          accepting_states)

  #
  # solution and search
  #
  db = solver.Phase(x, solver.INT_VAR_DEFAULT, solver.INT_VALUE_DEFAULT)

  solver.NewSearch(db)

  num_solutions = 0
  x_val = []
  while solver.NextSolution():
    num_solutions += 1
    x_val = [1] + [x[i].Value() for i in range(n)]
    print('x:', x_val)
    for i in range(1, n + 1):
      print('%s -> %s' % (nodes[x_val[i - 1] - 1], nodes[x_val[i] - 1]))

  solver.EndSearch()

  if num_solutions > 0:
    print()
    print('num_solutions:', num_solutions)
    print('failures:', solver.Failures())
    print('branches:', solver.Branches())
    print('WallTime:', solver.WallTime(), 'ms')

  # return the solution (or an empty array)
  return x_val


# Search for a minimum solution by increasing
# the length of the state array.
if __name__ == '__main__':
  for n in range(1, 15):
    result = main(n)
    result_len = len(result)
    if result_len:
      print()
      print('Found a solution of length %i:' % result_len, result)
      print()
      break
