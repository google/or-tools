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

  Global constraint contiguity using regularin Google CP Solver.

  This is a decomposition of the global constraint
  global contiguity.

  From Global Constraint Catalogue
  http://www.emn.fr/x-info/sdemasse/gccat/Cglobal_contiguity.html
  '''
  Enforce all variables of the VARIABLES collection to be assigned to 0 or 1.
  In addition, all variables assigned to value 1 appear contiguously.

  Example:
  (<0, 1, 1, 0>)

  The global_contiguity constraint holds since the sequence 0 1 1 0 contains
  no more than one group of contiguous 1.
  '''

  Compare with the following model:
  * MiniZinc: http://www.hakank.org/minizinc/contiguity_regular.mzn

  This model was created by Hakan Kjellerstrand (hakank@gmail.com)
  Also see my other Google CP Solver models:
  http://www.hakank.org/google_or_tools/

"""
from ortools.constraint_solver import pywrapcp

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


def main():

  # Create the solver.
  solver = pywrapcp.Solver('Global contiguity using regular')

  #
  # data
  #
  # the DFA (for regular)
  n_states = 3
  input_max = 2
  initial_state = 1  # 0 is for the failing state

  # all states are accepting states
  accepting_states = [1, 2, 3]

  # The regular expression 0*1*0*
  transition_fn = [
      [1, 2],  # state 1 (start): input 0 -> state 1, input 1 -> state 2 i.e. 0*
      [3, 2],  # state 2: 1*
      [3, 0],  # state 3: 0*
  ]

  n = 7

  #
  # declare variables
  #

  # We use 1..2 and subtract 1 in the solution
  reg_input = [solver.IntVar(1, 2, 'x[%i]' % i) for i in range(n)]

  #
  # constraints
  #
  regular(reg_input, n_states, input_max, transition_fn, initial_state,
          accepting_states)

  #
  # solution and search
  #
  db = solver.Phase(reg_input, solver.CHOOSE_FIRST_UNBOUND,
                    solver.ASSIGN_MIN_VALUE)

  solver.NewSearch(db)

  num_solutions = 0
  while solver.NextSolution():
    num_solutions += 1
    # Note: here we subract 1 from the solution
    print('reg_input:', [int(reg_input[i].Value() - 1) for i in range(n)])

  solver.EndSearch()
  print()
  print('num_solutions:', num_solutions)
  print('failures:', solver.Failures())
  print('branches:', solver.Branches())
  print('wall_time:', solver.WallTime(), 'ms')


if __name__ == '__main__':
  main()
