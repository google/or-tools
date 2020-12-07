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

  Nurse rostering in Google CP Solver.

  This is a simple nurse rostering model using a DFA and
  my decomposition of regular constraint.

  The DFA is from MiniZinc Tutorial, Nurse Rostering example:
  - one day off every 4 days
  - no 3 nights in a row.


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


def main():

  # Create the solver.
  solver = pywrapcp.Solver('Nurse rostering using regular')

  #
  # data
  #

  # Note: If you change num_nurses or num_days,
  #       please also change the constraints
  #       on nurse_stat and/or day_stat.
  num_nurses = 7
  num_days = 14

  day_shift = 1
  night_shift = 2
  off_shift = 3
  shifts = [day_shift, night_shift, off_shift]

  # the DFA (for regular)
  n_states = 6
  input_max = 3
  initial_state = 1  # 0 is for the failing state
  accepting_states = [1, 2, 3, 4, 5, 6]

  transition_fn = [
      # d,n,o
      [2, 3, 1],  # state 1
      [4, 4, 1],  # state 2
      [4, 5, 1],  # state 3
      [6, 6, 1],  # state 4
      [6, 0, 1],  # state 5
      [0, 0, 1]  # state 6
  ]

  days = ['d', 'n', 'o']  # for presentation

  #
  # declare variables
  #
  x = {}
  for i in range(num_nurses):
    for j in range(num_days):
      x[i, j] = solver.IntVar(shifts, 'x[%i,%i]' % (i, j))

  x_flat = [x[i, j] for i in range(num_nurses) for j in range(num_days)]

  # summary of the nurses
  nurse_stat = [
      solver.IntVar(0, num_days, 'nurse_stat[%i]' % i)
      for i in range(num_nurses)
  ]

  # summary of the shifts per day
  day_stat = {}
  for i in range(num_days):
    for j in shifts:
      day_stat[i, j] = solver.IntVar(0, num_nurses, 'day_stat[%i,%i]' % (i, j))

  day_stat_flat = [day_stat[i, j] for i in range(num_days) for j in shifts]

  #
  # constraints
  #
  for i in range(num_nurses):
    reg_input = [x[i, j] for j in range(num_days)]
    regular(reg_input, n_states, input_max, transition_fn, initial_state,
            accepting_states)

  #
  # Statistics and constraints for each nurse
  #
  for i in range(num_nurses):
    # number of worked days (day or night shift)
    b = [
        solver.IsEqualCstVar(x[i, j], day_shift) + solver.IsEqualCstVar(
            x[i, j], night_shift) for j in range(num_days)
    ]
    solver.Add(nurse_stat[i] == solver.Sum(b))

    # Each nurse must work between 7 and 10
    # days during this period
    solver.Add(nurse_stat[i] >= 7)
    solver.Add(nurse_stat[i] <= 10)

  #
  # Statistics and constraints for each day
  #
  for j in range(num_days):
    for t in shifts:
      b = [solver.IsEqualCstVar(x[i, j], t) for i in range(num_nurses)]
      solver.Add(day_stat[j, t] == solver.Sum(b))

    #
    # Some constraints for this day:
    #
    # Note: We have a strict requirements of
    #       the number of shifts.
    #       Using atleast constraints is much harder
    #       in this model.
    #
    if j % 7 == 5 or j % 7 == 6:
      # special constraints for the weekends
      solver.Add(day_stat[j, day_shift] == 2)
      solver.Add(day_stat[j, night_shift] == 1)
      solver.Add(day_stat[j, off_shift] == 4)
    else:
      # workdays:

      # - exactly 3 on day shift
      solver.Add(day_stat[j, day_shift] == 3)
      # - exactly 2 on night
      solver.Add(day_stat[j, night_shift] == 2)
      # - exactly 1 off duty
      solver.Add(day_stat[j, off_shift] == 2)

  #
  # solution and search
  #
  db = solver.Phase(day_stat_flat + x_flat + nurse_stat,
                    solver.CHOOSE_FIRST_UNBOUND, solver.ASSIGN_MIN_VALUE)

  solver.NewSearch(db)

  num_solutions = 0
  while solver.NextSolution():
    num_solutions += 1

    for i in range(num_nurses):
      print('Nurse%i: ' % i, end=' ')
      this_day_stat = defaultdict(int)
      for j in range(num_days):
        d = days[x[i, j].Value() - 1]
        this_day_stat[d] += 1
        print(d, end=' ')
      print(
          ' day_stat:', [(d, this_day_stat[d]) for d in this_day_stat], end=' ')
      print('total:', nurse_stat[i].Value(), 'workdays')
    print()

    print('Statistics per day:')
    for j in range(num_days):
      print('Day%2i: ' % j, end=' ')
      for t in shifts:
        print(day_stat[j, t].Value(), end=' ')
      print()
    print()

    # We just show 2 solutions
    if num_solutions >= 2:
      break

  solver.EndSearch()
  print()
  print('num_solutions:', num_solutions)
  print('failures:', solver.Failures())
  print('branches:', solver.Branches())
  print('WallTime:', solver.WallTime(), 'ms')


if __name__ == '__main__':
  main()
