# Copyright 2010 Google
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

  Hidato puzzle in Google CP Solver.

  http://www.shockwave.com/gamelanding/hidato.jsp
  http://www.hidato.com/
  '''
  Puzzles start semi-filled with numbered tiles.
  The first and last numbers are circled.
  Connect the numbers together to win. Consecutive
  number must touch horizontally, vertically, or
  diagonally.
  '''
"""

from constraint_solver import pywrapcp

def BuildTuples(r, c):
  results = []
  for x in range(r):
    for y in range(c):
      for dx in (-1, 0, 1):
        for dy in (-1, 0, 1):
          if (x + dx >= 0 and
              x + dx < r and
              y + dy >= 0 and
              y + dy < c and
              (dx != 0 or dy != 0)):
            results.append((x * c + y, (x + dx) * c + (y + dy)))
  return results

def main():

  # Create the solver.
  solver = pywrapcp.Solver('n-queens')

  #
  # data
  #
  #
  # Simple problem
  #
  # r = 3
  # c = r
  # puzzle = [
  #     [6,0,9],
  #     [0,2,8],
  #     [1,0,0]
  #     ]


  #     r = 7
  #     c = 7
  #     puzzle =  [
  #         [0,44,41, 0, 0, 0, 0],
  #         [0,43, 0,28,29, 0, 0],
  #         [0, 1, 0, 0, 0,33, 0],
  #         [0, 2,25, 4,34, 0,36],
  #         [49,16, 0,23, 0, 0, 0],
  #         [0,19, 0, 0,12, 7, 0],
  #         [0, 0, 0,14, 0, 0, 0]
  #         ]


  # Problems from the book:
  # Gyora Bededek: "Hidato: 2000 Pure Logic Puzzles"

  # Problem 1 (Practice)
  # r = 5
  # c = r
  # puzzle = [
  #    [ 0, 0,20, 0, 0],
  #    [ 0, 0, 0,16,18],
  #    [22, 0,15, 0, 0],
  #    [23, 0, 1,14,11],
  #    [ 0,25, 0, 0,12],
  #    ]


#     # problem 2 (Practice)
#  r = 5
#  c = r
#  puzzle= [
#      [0, 0, 0, 0,14],
#      [0,18,12, 0, 0],
#      [0, 0,17, 4, 5],
#      [0, 0, 7, 0, 0],
#      [9, 8,25, 1, 0],
#      ];

  # problem 3 (Beginner)
  #     r = 6
  #     c = r
  #     puzzle =  [
  #         [ 0, 26,0, 0, 0,18],
  #         [ 0, 0,27, 0, 0,19],
  #         [31,23, 0, 0,14, 0],
  #         [ 0,33, 8, 0,15, 1],
  #         [ 0, 0, 0, 5, 0, 0],
  #         [35,36, 0,10, 0, 0]
  #         ];


  # Problem 15 (Intermediate)
  # Note: This takes very long time to solve...
  r = 8
  c = r
  puzzle = [
      [64, 0, 0, 0, 0, 0, 0, 0],
      [ 1,63, 0,59,15,57,53, 0],
      [ 0, 4, 0,14, 0, 0, 0, 0],
      [ 3, 0,11, 0,20,19, 0,50],
      [ 0, 0, 0, 0,22, 0,48,40],
      [ 9, 0, 0,32,23, 0, 0,41],
      [27, 0, 0, 0,36, 0,46, 0],
      [28,30, 0,35, 0, 0, 0, 0]
      ]

  print_game(puzzle, r, c)

  #
  # declare variables
  #
  positions = [solver.IntVar(0, r * c - 1, 'p of %i' % i) for i in range(r * c)]

  #
  # constraints
  #
  solver.Add(solver.AllDifferent(positions, True))

  #
  # Fill in the clues
  #
  for i in range(r):
    for j in range(c):
      if puzzle[i][j] > 0:
        solver.Add(positions[puzzle[i][j] - 1] == i * c + j)

  # Positions are closed another. Use a table.
  close_tuples = BuildTuples(r, c)
  for k in range(1, r * c - 1):
    solver.Add(solver.AllowedAssignments((positions[k], positions[k + 1]),
                                         close_tuples))

  #
  # solution and search
  #

  # db: DecisionBuilder
  db = solver.Phase(positions,
                    solver.CHOOSE_MIN_SIZE_LOWEST_MIN,
                    solver.ASSIGN_MIN_VALUE)

  solver.NewSearch(db)
  num_solutions = 0
  while solver.NextSolution():
    num_solutions += 1
    print_board(positions, r, c, num_solutions)
    print

  solver.EndSearch()

  print
  print "num_solutions:", num_solutions
  print "failures:", solver.failures()
  print "branches:", solver.branches()
  print "wall_time:", solver.wall_time()


def print_board(positions, rows, cols, num_solution):
  print 'Solution %i:' % num_solution
  for i in range(rows):
    for j in range(cols):
      index = i * rows + j
      for k in range(rows * cols):
        if positions[k].Value() == index:
          print "% 2s" % (k + 1),
    print ''

def print_game(game, rows, cols):
  print 'Initial game (%i x %i)' % (rows, cols)
  for i in range(rows):
    for j in range(cols):
      print "% 2s" % game[i][j],
    print ''
  print



if __name__ == '__main__':
    main()
