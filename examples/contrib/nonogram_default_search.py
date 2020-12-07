# Copyright 2010 Hakan Kjellerstrand hakank@gmail.com, lperron@google.com
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

"""
import sys

from ortools.constraint_solver import pywrapcp


#
# Make a transition (automaton) list of tuples from a
# single pattern, e.g. [3,2,1]
#
def make_transition_tuples(pattern):
  p_len = len(pattern)
  num_states = p_len + sum(pattern)

  tuples = []

  # this is for handling 0-clues. It generates
  # just the minimal state
  if num_states == 0:
    tuples.append((1, 0, 1))
    return (tuples, 1)

  # convert pattern to a 0/1 pattern for easy handling of
  # the states
  tmp = [0]
  c = 0
  for pattern_index in range(p_len):
    tmp.extend([1] * pattern[pattern_index])
    tmp.append(0)

  for i in range(num_states):
    state = i + 1
    if tmp[i] == 0:
      tuples.append((state, 0, state))
      tuples.append((state, 1, state + 1))
    else:
      if i < num_states - 1:
        if tmp[i + 1] == 1:
          tuples.append((state, 1, state + 1))
        else:
          tuples.append((state, 0, state + 1))
  tuples.append((num_states, 0, num_states))
  return (tuples, num_states)


#
# check each rule by creating an automaton and transition constraint.
#
def check_rule(rules, y):
  cleaned_rule = [rules[i] for i in range(len(rules)) if rules[i] > 0]
  (transition_tuples, last_state) = make_transition_tuples(cleaned_rule)

  initial_state = 1
  accepting_states = [last_state]

  solver = y[0].solver()
  solver.Add(
      solver.TransitionConstraint(y, transition_tuples, initial_state,
                                  accepting_states))


def main(rows, row_rule_len, row_rules, cols, col_rule_len, col_rules):

  # Create the solver.
  solver = pywrapcp.Solver('Nonogram')

  #
  # variables
  #
  board = {}
  for i in range(rows):
    for j in range(cols):
      board[i, j] = solver.IntVar(0, 1, 'board[%i, %i]' % (i, j))

  board_flat = [board[i, j] for i in range(rows) for j in range(cols)]

  # Flattened board for labeling.
  # This labeling was inspired by a suggestion from
  # Pascal Van Hentenryck about my (hakank's) Comet
  # nonogram model.
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
    check_rule(row_rules[i], [board[i, j] for j in range(cols)])

  for j in range(cols):
    check_rule(col_rules[j], [board[i, j] for i in range(rows)])

  #
  # solution and search
  #
  parameters = pywrapcp.DefaultPhaseParameters()
  parameters.heuristic_period = 200000

  db = solver.DefaultPhase(board_label, parameters)

  print('before solver, wall time = ', solver.WallTime(), 'ms')
  solver.NewSearch(db)

  num_solutions = 0
  while solver.NextSolution():
    print()
    num_solutions += 1
    for i in range(rows):
      row = [board[i, j].Value() for j in range(cols)]
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
