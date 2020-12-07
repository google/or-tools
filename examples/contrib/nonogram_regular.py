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

  Nonogram  (Painting by numbers) in Google CP Solver.

  http://en.wikipedia.org/wiki/Nonogram
  '''
  Nonograms or Paint by Numbers are picture logic puzzles in which cells in a
  grid have to be colored or left blank according to numbers given at the
  side of the grid to reveal a hidden picture. In this puzzle type, the
  numbers measure how many unbroken lines of filled-in squares there are
  in any given row or column. For example, a clue of '4 8 3' would mean
  there are sets of four, eight, and three filled squares, in that order,
  with at least one blank square between successive groups.

  '''

  See problem 12 at http://www.csplib.org/.

  http://www.puzzlemuseum.com/nonogram.htm

  Haskell solution:
  http://twan.home.fmf.nl/blog/haskell/Nonograms.details

  Brunetti, Sara & Daurat, Alain (2003)
  'An algorithm reconstructing convex lattice sets'
  http://geodisi.u-strasbg.fr/~daurat/papiers/tomoqconv.pdf


  The Comet model (http://www.hakank.org/comet/nonogram_regular.co)
  was a major influence when writing this Google CP solver model.

  I have also blogged about the development of a Nonogram solver in Comet
  using the regular constraint.
  * 'Comet: Nonogram improved: solving problem P200 from 1:30 minutes
     to about 1 second'
     http://www.hakank.org/constraint_programming_blog/2009/03/comet_nonogram_improved_solvin_1.html

  * 'Comet: regular constraint, a much faster Nonogram with the regular
  constraint,
     some OPL models, and more'
     http://www.hakank.org/constraint_programming_blog/2009/02/comet_regular_constraint_a_muc_1.html

  Compare with the other models:
  * Gecode/R: http://www.hakank.org/gecode_r/nonogram.rb (using 'regexps')
  * MiniZinc: http://www.hakank.org/minizinc/nonogram_regular.mzn
  * MiniZinc: http://www.hakank.org/minizinc/nonogram_create_automaton.mzn
  * MiniZinc: http://www.hakank.org/minizinc/nonogram_create_automaton2.mzn
    Note: nonogram_create_automaton2.mzn is the preferred model

  This model was created by Hakan Kjellerstrand (hakank@gmail.com)
  Also see my other Google CP Solver models:
  http://www.hakank.org/google_or_tools/

"""
import sys

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

  # int d2[0..Q, 1..S]
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
  num_states = p_len + sum(pattern)

  # this is for handling 0-clues. It generates
  # just the state 1,2
  if num_states == 0:
    num_states = 1

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

  # print 'The states:'
  # for i in range(num_states):
  #     for j in range(2):
  #         print t_matrix[i][j],
  #     print
  # print

  return t_matrix


#
# check each rule by creating an automaton
# and regular
#


def check_rule(rules, y):
  solver = y[0].solver()

  r_len = sum([1 for i in range(len(rules)) if rules[i] > 0])
  rules_tmp = []
  for i in range(len(rules)):
    if rules[i] > 0:
      rules_tmp.append(rules[i])

  transition_fn = make_transition_matrix(rules_tmp)
  n_states = len(transition_fn)
  input_max = 2

  # Note: we cannot use 0 since it's the failing state
  initial_state = 1
  accepting_states = [n_states]  # This is the last state

  regular(y, n_states, input_max, transition_fn, initial_state,
          accepting_states)


def main(rows, row_rule_len, row_rules, cols, col_rule_len, col_rules):

  # Create the solver.
  solver = pywrapcp.Solver('Regular test')

  #
  # data
  #

  #
  # variables
  #
  board = {}
  for i in range(rows):
    for j in range(cols):
      board[i, j] = solver.IntVar(1, 2, 'board[%i,%i]' % (i, j))
  board_flat = [board[i, j] for i in range(rows) for j in range(cols)]

  # Flattened board for labeling.
  # This labeling was inspired by a suggestion from
  # Pascal Van Hentenryck about my Comet nonogram model.
  board_label = []
  if rows * row_rule_len < cols * col_rule_len:
    for i in range(rows):
      for j in range(cols):
        board_label.append(board[i, j])
  else:
    for j in range(cols):
      for i in range(rows):
        board_label.append(board[i, j])

  #
  # constraints
  #
  for i in range(rows):
    check_rule([row_rules[i][j] for j in range(row_rule_len)],
               [board[i, j] for j in range(cols)])

  for j in range(cols):
    check_rule([col_rules[j][k] for k in range(col_rule_len)],
               [board[i, j] for i in range(rows)])

  #
  # solution and search
  #
  db = solver.Phase(board_label, solver.CHOOSE_FIRST_UNBOUND,
                    solver.ASSIGN_MIN_VALUE)

  solver.NewSearch(db)

  num_solutions = 0
  while solver.NextSolution():
    print()
    num_solutions += 1
    for i in range(rows):
      row = [board[i, j].Value() - 1 for j in range(cols)]
      row_pres = []
      for j in row:
        if j == 1:
          row_pres.append('#')
        else:
          row_pres.append(' ')
      print('  ', ''.join(row_pres))

    print()
    print('  ', '-' * cols)

    if num_solutions >= 2:
      print('2 solutions is enough...')
      break

  solver.EndSearch()
  print()
  print('num_solutions:', num_solutions)
  print('failures:', solver.Failures())
  print('branches:', solver.Branches())
  print('WallTime:', solver.WallTime(), 'ms')


#
# Default problem
#
# From http://twan.home.fmf.nl/blog/haskell/Nonograms.details
# The lambda picture
#
rows = 12
row_rule_len = 3
row_rules = [[0, 0, 2], [0, 1, 2], [0, 1, 1], [0, 0, 2], [0, 0, 1], [0, 0, 3],
             [0, 0, 3], [0, 2, 2], [0, 2, 1], [2, 2, 1], [0, 2, 3], [0, 2, 2]]

cols = 10
col_rule_len = 2
col_rules = [[2, 1], [1, 3], [2, 4], [3, 4], [0, 4], [0, 3], [0, 3], [0, 3],
             [0, 2], [0, 2]]

if __name__ == '__main__':
  if len(sys.argv) > 1:
    file = sys.argv[1]
    exec(compile(open(file).read(), file, 'exec'))
  main(rows, row_rule_len, row_rules, cols, col_rule_len, col_rules)
