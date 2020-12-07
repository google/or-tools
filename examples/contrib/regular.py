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

  Global constraint regular in Google CP Solver.

  This is a translation of MiniZinc's regular constraint (defined in
  lib/zinc/globals.mzn). All comments are from the MiniZinc code.
  '''
  The sequence of values in array 'x' (which must all be in the range 1..S)
  is accepted by the DFA of 'Q' states with input 1..S and transition
  function 'd' (which maps (1..Q, 1..S) -> 0..Q)) and initial state 'q0'
  (which must be in 1..Q) and accepting states 'F' (which all must be in
  1..Q).  We reserve state 0 to be an always failing state.
  '''

  It is, however, translated from the Comet model:
  * Comet: http://www.hakank.org/comet/regular.co

  Here we test with the following regular expression:
    0*1{3}0+1{2}0+1{1}0*
  using an array of size 10.

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

  # int d2[0..Q, 1..S];
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


#
# Make a transition (automaton) matrix from a
# single pattern, e.g. [3,2,1]
#
def make_transition_matrix(pattern):

  p_len = len(pattern)
  print('p_len:', p_len)
  num_states = p_len + sum(pattern)
  print('num_states:', num_states)
  t_matrix = []
  for i in range(num_states):
    row = []
    for j in range(2):
      row.append(0)
    t_matrix.append(row)

  # convert pattern to a 0/1 pattern for easy handling of
  # the states
  tmp = [0 for i in range(num_states)]
  c = 0
  tmp[c] = 0
  for i in range(p_len):
    for j in range(pattern[i]):
      c += 1
      tmp[c] = 1
    if c < num_states - 1:
      c += 1
      tmp[c] = 0
  print('tmp:', tmp)

  t_matrix[num_states - 1][0] = num_states
  t_matrix[num_states - 1][1] = 0

  for i in range(num_states):
    if tmp[i] == 0:
      t_matrix[i][0] = i + 1
      t_matrix[i][1] = i + 2
    else:
      if i < num_states - 1:
        if tmp[i + 1] == 1:
          t_matrix[i][0] = 0
          t_matrix[i][1] = i + 2
        else:
          t_matrix[i][0] = i + 2
          t_matrix[i][1] = 0

  print('The states:')
  for i in range(num_states):
    for j in range(2):
      print(t_matrix[i][j], end=' ')
    print()
  print()

  return t_matrix


def main():

  # Create the solver.
  solver = pywrapcp.Solver('Regular test')

  #
  # data
  #

  this_len = 10
  pp = [3, 2, 1]

  transition_fn = make_transition_matrix(pp)
  n_states = len(transition_fn)
  input_max = 2

  # Note: we use '1' and '2' (rather than 0 and 1)
  # since 0 represents the failing state.
  initial_state = 1

  accepting_states = [n_states]

  # declare variables
  reg_input = [
      solver.IntVar(1, input_max, 'reg_input[%i]' % i) for i in range(this_len)
  ]

  #
  # constraints
  #
  regular(reg_input, n_states, input_max, transition_fn, initial_state,
          accepting_states)

  #
  # solution and search
  #
  db = solver.Phase(reg_input, solver.CHOOSE_MIN_SIZE_HIGHEST_MAX,
                    solver.ASSIGN_MIN_VALUE)

  solver.NewSearch(db)

  num_solutions = 0
  while solver.NextSolution():
    print('reg_input:', [reg_input[i].Value() - 1 for i in range(this_len)])
    num_solutions += 1

  solver.EndSearch()
  print()
  print('num_solutions:', num_solutions)
  print('failures:', solver.Failures())
  print('branches:', solver.Branches())
  print('WallTime:', solver.WallTime(), 'ms')


if __name__ == '__main__':
  main()
