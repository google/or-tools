#!/usr/bin/env python3
# Copyright 2010-2024 Google LLC
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

from ortools.sat.python import cp_model


# pylint: disable=too-many-statements
def solve_zebra():
    """Solves the zebra problem."""

    # Create the model.
    model = cp_model.CpModel()

    red = model.new_int_var(1, 5, "red")
    green = model.new_int_var(1, 5, "green")
    yellow = model.new_int_var(1, 5, "yellow")
    blue = model.new_int_var(1, 5, "blue")
    ivory = model.new_int_var(1, 5, "ivory")

    englishman = model.new_int_var(1, 5, "englishman")
    spaniard = model.new_int_var(1, 5, "spaniard")
    japanese = model.new_int_var(1, 5, "japanese")
    ukrainian = model.new_int_var(1, 5, "ukrainian")
    norwegian = model.new_int_var(1, 5, "norwegian")

    dog = model.new_int_var(1, 5, "dog")
    snails = model.new_int_var(1, 5, "snails")
    fox = model.new_int_var(1, 5, "fox")
    zebra = model.new_int_var(1, 5, "zebra")
    horse = model.new_int_var(1, 5, "horse")

    tea = model.new_int_var(1, 5, "tea")
    coffee = model.new_int_var(1, 5, "coffee")
    water = model.new_int_var(1, 5, "water")
    milk = model.new_int_var(1, 5, "milk")
    fruit_juice = model.new_int_var(1, 5, "fruit juice")

    old_gold = model.new_int_var(1, 5, "old gold")
    kools = model.new_int_var(1, 5, "kools")
    chesterfields = model.new_int_var(1, 5, "chesterfields")
    lucky_strike = model.new_int_var(1, 5, "lucky strike")
    parliaments = model.new_int_var(1, 5, "parliaments")

    model.add_all_different(red, green, yellow, blue, ivory)
    model.add_all_different(englishman, spaniard, japanese, ukrainian, norwegian)
    model.add_all_different(dog, snails, fox, zebra, horse)
    model.add_all_different(tea, coffee, water, milk, fruit_juice)
    model.add_all_different(parliaments, kools, chesterfields, lucky_strike, old_gold)

    model.add(englishman == red)
    model.add(spaniard == dog)
    model.add(coffee == green)
    model.add(ukrainian == tea)
    model.add(green == ivory + 1)
    model.add(old_gold == snails)
    model.add(kools == yellow)
    model.add(milk == 3)
    model.add(norwegian == 1)

    diff_fox_chesterfields = model.new_int_var(-4, 4, "diff_fox_chesterfields")
    model.add(diff_fox_chesterfields == fox - chesterfields)
    model.add_abs_equality(1, diff_fox_chesterfields)

    diff_horse_kools = model.new_int_var(-4, 4, "diff_horse_kools")
    model.add(diff_horse_kools == horse - kools)
    model.add_abs_equality(1, diff_horse_kools)

    model.add(lucky_strike == fruit_juice)
    model.add(japanese == parliaments)

    diff_norwegian_blue = model.new_int_var(-4, 4, "diff_norwegian_blue")
    model.add(diff_norwegian_blue == norwegian - blue)
    model.add_abs_equality(1, diff_norwegian_blue)

    # Solve and print out the solution.
    solver = cp_model.CpSolver()
    status = solver.solve(model)

    if status == cp_model.OPTIMAL:
        people = [englishman, spaniard, japanese, ukrainian, norwegian]
        water_drinker = [p for p in people if solver.value(p) == solver.value(water)][0]
        zebra_owner = [p for p in people if solver.value(p) == solver.value(zebra)][0]
        print("The", water_drinker.name, "drinks water.")
        print("The", zebra_owner.name, "owns the zebra.")
    else:
        print("No solutions to the zebra problem, this is unusual!")


solve_zebra()
