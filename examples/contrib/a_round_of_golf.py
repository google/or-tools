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

  A Round of Golf puzzle (Dell Logic Puzzles) in Google CP Solver.

  From http://brownbuffalo.sourceforge.net/RoundOfGolfClues.html
  '''
  Title: A Round of Golf
  Author: Ellen K. Rodehorst
  Publication: Dell Favorite Logic Problems
  Issue: Summer, 2000
  Puzzle #: 9
  Stars: 1

  When the Sunny Hills Country Club golf course isn't in use by club members,
  of course, it's open to the club's employees. Recently, Jack and three other
  workers at the golf course got together on their day off to play a round of
  eighteen holes of golf.
  Afterward, all four, including Mr. Green, went to the clubhouse to total
  their scorecards. Each man works at a different job (one is a short-order
  cook), and each shot a different score in the game. No one scored below
  70 or above 85 strokes. From the clues below, can you discover each man's
  full name, job and golf score?

  1. Bill, who is not the maintenance man, plays golf often and had the lowest
  score of the foursome.
  2. Mr. Clubb, who isn't Paul, hit several balls into the woods and scored ten
  strokes more than the pro-shop clerk.
  3. In some order, Frank and the caddy scored four and seven more strokes than
  Mr. Sands.
  4. Mr. Carter thought his score of 78 was one of his better games, even
     though Frank's score  was lower.
  5. None of the four scored exactly 81 strokes.

  Determine: First Name - Last Name - Job - Score
  '''

  Compare with the F1 model:
  http://www.f1compiler.com/samples/A 20Round 20of 20Golf.f1.html


  Compare with the following models:
  * MiniZinc: http://www.hakank.org/minizinc/a_round_of_golf.mzn
  * Comet   : http://www.hakank.org/comet/a_round_of_golf.co
  * ECLiPSe : http://www.hakank.org/eclipse/a_round_of_golf.ecl
  * Gecode  :  http://hakank.org/gecode/a_round_of_golf.cpp
  * SICStus : http://hakank.org/sicstus/a_round_of_golf.pl

  This model was created by Hakan Kjellerstrand (hakank@gmail.com)
  Also see my other Google CP Solver models:
  http://www.hakank.org/google_or_tools/

"""


from ortools.constraint_solver import pywrapcp


def main():

  # Create the solver.
  solver = pywrapcp.Solver("All interval")

  #
  # data
  #
  n = 4
  [Jack, Bill, Paul, Frank] = [i for i in range(n)]

  #
  # declare variables
  #
  last_name = [solver.IntVar(0, n - 1, "last_name[%i]" % i) for i in range(n)]
  [Green, Clubb, Sands, Carter] = last_name

  job = [solver.IntVar(0, n - 1, "job[%i]" % i) for i in range(n)]
  [cook, maintenance_man, clerk, caddy] = job

  score = [solver.IntVar(70, 85, "score[%i]" % i) for i in range(n)]

  #
  # constraints
  #
  solver.Add(solver.AllDifferent(last_name))
  solver.Add(solver.AllDifferent(job))
  solver.Add(solver.AllDifferent(score))

  # 1. Bill, who is not the maintenance man, plays golf often and had
  #    the lowest score of the foursome.
  solver.Add(Bill != maintenance_man)
  solver.Add(score[Bill] < score[Jack])
  solver.Add(score[Bill] < score[Paul])
  solver.Add(score[Bill] < score[Frank])

  # 2. Mr. Clubb, who isn't Paul, hit several balls into the woods and
  #    scored ten strokes more than the pro-shop clerk.
  solver.Add(Clubb != Paul)
  solver.Add(solver.Element(score, Clubb) == solver.Element(score, clerk) + 10)

  # 3. In some order, Frank and the caddy scored four and seven more
  #    strokes than Mr. Sands.
  solver.Add(Frank != caddy)
  solver.Add(Frank != Sands)
  solver.Add(caddy != Sands)

  b3_a_1 = solver.IsEqualVar(solver.Element(score, Sands) + 4, score[Frank])
  b3_a_2 = solver.IsEqualVar(
      solver.Element(score, caddy),
      solver.Element(score, Sands) + 7)

  b3_b_1 = solver.IsEqualVar(solver.Element(score, Sands) + 7, score[Frank])
  b3_b_2 = solver.IsEqualVar(
      solver.Element(score, caddy),
      solver.Element(score, Sands) + 4)

  solver.Add((b3_a_1 * b3_a_2) + (b3_b_1 * b3_b_2) == 1)

  # 4. Mr. Carter thought his score of 78 was one of his better games,
  #    even though Frank's score was lower.
  solver.Add(Frank != Carter)
  solver.Add(solver.Element(score, Carter) == 78)
  solver.Add(score[Frank] < solver.Element(score, Carter))

  # 5. None of the four scored exactly 81 strokes.
  [solver.Add(score[i] != 81) for i in range(n)]

  #
  # solution and search
  #
  solution = solver.Assignment()
  solution.Add(last_name)
  solution.Add(job)
  solution.Add(score)

  db = solver.Phase(last_name + job + score, solver.CHOOSE_FIRST_UNBOUND,
                    solver.INT_VALUE_DEFAULT)

  solver.NewSearch(db)
  num_solutions = 0
  while solver.NextSolution():
    print("last_name:", [last_name[i].Value() for i in range(n)])
    print("job      :", [job[i].Value() for i in range(n)])
    print("score    :", [score[i].Value() for i in range(n)])
    num_solutions += 1
    print()

  print("num_solutions:", num_solutions)
  print("failures:", solver.Failures())
  print("branches:", solver.Branches())
  print("WallTime:", solver.WallTime())


if __name__ == "__main__":
  main()
