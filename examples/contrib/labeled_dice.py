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

  Labeled dice problem in Google CP Solver.

  From Jim Orlin 'Colored letters, labeled dice: a logic puzzle'
  http://jimorlin.wordpress.com/2009/02/17/colored-letters-labeled-dice-a-logic-puzzle/
  '''
  My daughter Jenn bough a puzzle book, and showed me a cute puzzle.  There
  are 13 words as follows:  BUOY, CAVE, CELT, FLUB, FORK, HEMP, JUDY,
  JUNK, LIMN, QUIP, SWAG, VISA, WISH.

  There are 24 different letters that appear in the 13 words.  The question
  is:  can one assign the 24 letters to 4 different cubes so that the
  four letters of each word appears on different cubes.  (There is one
  letter from each word on each cube.)  It might be fun for you to try
  it.  I'll give a small hint at the end of this post. The puzzle was
  created by Humphrey Dudley.
  '''

  Jim Orlin's followup 'Update on Logic Puzzle':
  http://jimorlin.wordpress.com/2009/02/21/update-on-logic-puzzle/


  Compare with the following models:
  * ECLiPSe: http://hakank.org/eclipse/labeled_dice.ecl
  * Comet  : http://www.hakank.org/comet/labeled_dice.co
  * Gecode : http://hakank.org/gecode/labeled_dice.cpp
  * SICStus: http://hakank.org/sicstus/labeled_dice.pl
  * Zinc   : http://hakank.org/minizinc/labeled_dice.zinc


  This model was created by Hakan Kjellerstrand (hakank@gmail.com)
  Also see my other Google CP Solver models:
  http://www.hakank.org/google_or_tools/
"""
from ortools.constraint_solver import pywrapcp


def main():

  # Create the solver.
  solver = pywrapcp.Solver("Labeled dice")

  #
  # data
  #
  n = 4
  m = 24
  A, B, C, D, E, F, G, H, I, J, K, L, M, N, O, P, Q, R, S, T, U, V, W, Y = (
      list(range(m)))
  letters = [
      "A", "B", "C", "D", "E", "F", "G", "H", "I", "J", "K", "L", "M", "N", "O",
      "P", "Q", "R", "S", "T", "U", "V", "W", "Y"
  ]

  num_words = 13
  words = [[B, U, O, Y], [C, A, V, E], [C, E, L, T], [F, L, U, B], [F, O, R, K],
           [H, E, M, P], [J, U, D, Y], [J, U, N, K], [L, I, M, N], [Q, U, I, P],
           [S, W, A, G], [V, I, S, A], [W, I, S, H]]

  #
  # declare variables
  #
  dice = [solver.IntVar(0, n - 1, "dice[%i]" % i) for i in range(m)]

  #
  # constraints
  #

  # the letters in a word must be on a different die
  for i in range(num_words):
    solver.Add(solver.AllDifferent([dice[words[i][j]] for j in range(n)]))

  # there must be exactly 6 letters of each die
  for i in range(n):
    b = [solver.IsEqualCstVar(dice[j], i) for j in range(m)]
    solver.Add(solver.Sum(b) == 6)

  #
  # solution and search
  #
  solution = solver.Assignment()
  solution.Add(dice)

  db = solver.Phase(dice, solver.CHOOSE_FIRST_UNBOUND, solver.ASSIGN_MIN_VALUE)

  #
  # result
  #
  solver.NewSearch(db)
  num_solutions = 0
  while solver.NextSolution():
    num_solutions += 1
    # print "dice:", [(letters[i],dice[i].Value()) for i in range(m)]
    for d in range(n):
      print("die %i:" % d, end=" ")
      for i in range(m):
        if dice[i].Value() == d:
          print(letters[i], end=" ")
      print()

    print("The words with the cube label:")
    for i in range(num_words):
      for j in range(n):
        print(
            "%s (%i)" % (letters[words[i][j]], dice[words[i][j]].Value()),
            end=" ")
      print()

    print()

  solver.EndSearch()

  print()
  print("num_solutions:", num_solutions)
  print("failures:", solver.Failures())
  print("branches:", solver.Branches())
  print("WallTime:", solver.WallTime())


if __name__ == "__main__":
  main()
