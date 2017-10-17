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

"""This is the zebra problem as invented by Lewis Caroll.

There are five houses.
The Englishman lives in the red house.
The Spaniard owns the dog.
Coffee is drunk in the green house.
The Ukrainian drinks tea.
The green house is immediately to the right of the ivory house.
The Old Gold smoker owns snails.
Kools are smoked in the yellow house.
Milk is drunk in the middle house.
The Norwegian lives in the first house.
The man who smokes Chesterfields lives in the house next to the man
   with the fox.
Kools are smoked in the house next to the house where the horse is kept.
The Lucky Strike smoker drinks orange juice.
The Japanese smokes Parliaments.
The Norwegian lives next to the blue house.

Who owns a zebra and who drinks water?
"""

from __future__ import print_function
from ortools.constraint_solver import pywrapcp


# pylint: disable=too-many-statements
def main():
  # Create the solver.
  solver = pywrapcp.Solver('zebra')

  red = solver.IntVar(1, 5, 'red')
  green = solver.IntVar(1, 5, 'green')
  yellow = solver.IntVar(1, 5, 'yellow')
  blue = solver.IntVar(1, 5, 'blue')
  ivory = solver.IntVar(1, 5, 'ivory')

  englishman = solver.IntVar(1, 5, 'englishman')
  spaniard = solver.IntVar(1, 5, 'spaniard')
  japanese = solver.IntVar(1, 5, 'japanese')
  ukrainian = solver.IntVar(1, 5, 'ukrainian')
  norwegian = solver.IntVar(1, 5, 'norwegian')

  dog = solver.IntVar(1, 5, 'dog')
  snails = solver.IntVar(1, 5, 'snails')
  fox = solver.IntVar(1, 5, 'fox')
  zebra = solver.IntVar(1, 5, 'zebra')
  horse = solver.IntVar(1, 5, 'horse')

  tea = solver.IntVar(1, 5, 'tea')
  coffee = solver.IntVar(1, 5, 'coffee')
  water = solver.IntVar(1, 5, 'water')
  milk = solver.IntVar(1, 5, 'milk')
  fruit_juice = solver.IntVar(1, 5, 'fruit juice')

  old_gold = solver.IntVar(1, 5, 'old gold')
  kools = solver.IntVar(1, 5, 'kools')
  chesterfields = solver.IntVar(1, 5, 'chesterfields')
  lucky_strike = solver.IntVar(1, 5, 'lucky strike')
  parliaments = solver.IntVar(1, 5, 'parliaments')

  solver.Add(solver.AllDifferent([red, green, yellow, blue, ivory]))
  solver.Add(solver.AllDifferent([englishman, spaniard, japanese,
                                  ukrainian, norwegian]))
  solver.Add(solver.AllDifferent([dog, snails, fox, zebra, horse]))
  solver.Add(solver.AllDifferent([tea, coffee, water, milk,
                                  fruit_juice]))
  solver.Add(solver.AllDifferent([parliaments, kools, chesterfields,
                                  lucky_strike, old_gold]))

  solver.Add(englishman == red)
  solver.Add(spaniard == dog)
  solver.Add(coffee == green)
  solver.Add(ukrainian == tea)
  solver.Add(green == ivory + 1)
  solver.Add(old_gold == snails)
  solver.Add(kools == yellow)
  solver.Add(milk == 3)
  solver.Add(norwegian == 1)
  solver.Add(abs(fox - chesterfields) == 1)
  solver.Add(abs(horse - kools) == 1)
  solver.Add(lucky_strike == fruit_juice)
  solver.Add(japanese == parliaments)
  solver.Add(abs(norwegian - blue) == 1)

  all_vars = [parliaments, kools, chesterfields, lucky_strike, old_gold,
              englishman, spaniard, japanese, ukrainian, norwegian,
              dog, snails, fox, zebra, horse,
              tea, coffee, water, milk, fruit_juice,
              red, green, yellow, blue, ivory]

  solver.NewSearch(solver.Phase(all_vars,
                                solver.INT_VAR_DEFAULT,
                                solver.INT_VALUE_DEFAULT))
  if solver.NextSolution():
    people = [englishman, spaniard, japanese, ukrainian, norwegian]
    water_drinker = [p for p in people if p.Value() == water.Value()][0]
    zebra_owner = [p for p in people if p.Value() == zebra.Value()][0]
    print('The', water_drinker.Name(), 'drinks water.')
    print('The', zebra_owner.Name(), 'owns the zebra.')
  else:
    print('No solutions to the zebra problem, this is unusual!')
  solver.EndSearch()


if __name__ == '__main__':
  main()
