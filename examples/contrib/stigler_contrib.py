# Copyright 2011 Hakan Kjellerstrand hakank@gmail.com
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

  Original Stigler's 1939 diet problem Google or-tools.

  From GLPK:s example stigler.mod
  '''
  STIGLER, original Stigler's 1939 diet problem

  The Stigler Diet is an optimization problem named for George Stigler,
  a 1982 Nobel Laureate in economics, who posed the following problem:
  For a moderately active man weighing 154 pounds, how much of each of
  77 foods should be eaten on a daily basis so that the man's intake of
  nine nutrients will be at least equal to the recommended dietary
  allowances (RDSs) suggested by the National Research Council in 1943,
  with the cost of the diet being minimal?

  The nutrient RDAs required to be met in Stigler's experiment were
  calories, protein, calcium, iron, vitamin A, thiamine, riboflavin,
  niacin, and ascorbic acid. The result was an annual budget allocated
  to foods such as evaporated milk, cabbage, dried navy beans, and beef
  liver at a cost of approximately $0.11 a day in 1939 U.S. dollars.

  While the name 'Stigler Diet' was applied after the experiment by
  outsiders, according to Stigler, 'No one recommends these diets for
  anyone, let alone everyone.' The Stigler diet has been much ridiculed
  for its lack of variety and palatability, however his methodology has
  received praise and is considered to be some of the earliest work in
  linear programming.

  The Stigler diet question is a linear programming problem. Lacking
  any sophisticated method of solving such a problem, Stigler was
  forced to utilize heuristic methods in order to find a solution. The
  diet question originally asked in which quantities a 154 pound male
  would have to consume 77 different foods in order to fulfill the
  recommended intake of 9 different nutrients while keeping expense at
  a minimum. Through 'trial and error, mathematical insight and
  agility,' Stigler was able to eliminate 62 of the foods from the
  original 77 (these foods were removed based because they lacked
  nutrients in comparison to the remaining 15). From the reduced list,
  Stigler calculated the required amounts of each of the remaining 15
  foods to arrive at a cost-minimizing solution to his question.
  According to Stigler's calculations, the annual cost of his solution
  was $39.93 in 1939 dollars. When corrected for inflation using the
  consumer price index, the cost of the diet in 2005 dollars is
  $561.43. The specific combination of foods and quantities is as
  follows:

  Stigler's 1939 Diet

  Food             Annual Quantities Annual Cost
  ---------------- ----------------- -----------
  Wheat Flour           370 lb.         $13.33
  Evaporated Milk        57 cans          3.84
  Cabbage               111 lb.           4.11
  Spinach                23 lb.           1.85
  Dried Navy Beans      285 lb.          16.80
  ----------------------------------------------
  Total Annual Cost                     $39.93

  The 9 nutrients that Stigler's diet took into consideration and their
  respective recommended daily amounts were:

  Table of nutrients considered in Stigler's diet

  Nutrient                  Daily Recommended Intake
  ------------------------- ------------------------
  Calories                       3,000 Calories
  Protein                           70 grams
  Calcium                           .8 grams
  Iron                              12 milligrams
  Vitamin A                      5,000 IU
  Thiamine (Vitamin B1)            1.8 milligrams
  Riboflavin (Vitamin B2)          2.7 milligrams
  Niacin                            18 milligrams
  Ascorbic Acid (Vitamin C)         75 milligrams

  Seven years after Stigler made his initial estimates, the development
  of George Dantzig's Simplex algorithm made it possible to solve the
  problem without relying on heuristic methods. The exact value was
  determined to be $39.69 (using the original 1939 data). Dantzig's
  algorithm describes a method of traversing the vertices of a polytope
  of N+1 dimensions in order to find the optimal solution to a specific
  situation.

  (From Wikipedia, the free encyclopedia.)

  Translated from GAMS by Andrew Makhorin <mao@mai2.rcnet.ru>.

  For the original GAMS model stigler1939.gms see [3].

  References:

  1. George J. Stigler, 'The Cost of Subsistence,' J. Farm Econ. 27,
     1945, pp. 303-14.

  2. National Research Council, 'Recommended Daily Allowances,' Reprint
     and Circular Series No. 115, January, 1943.

  3. Erwin Kalvelagen, 'Model building with GAMS,' Chapter 2, 'Building
     linear programming models,' pp. 128-34.
  '''

  This model was created by Hakan Kjellerstrand (hakank@gmail.com)
  Also see my other Google CP Solver models:
  http://www.hakank.org/google_or_tools/
"""
import sys
from ortools.linear_solver import pywraplp


def main(sol="CBC"):

  # Create the solver.

  print("Solver: ", sol)

  # using GLPK
  if sol == "GLPK":
    solver = pywraplp.Solver("CoinsGridGLPK",
                             pywraplp.Solver.GLPK_MIXED_INTEGER_PROGRAMMING)
  else:
    # Using CLP
    solver = pywraplp.Solver("CoinsGridCLP",
                             pywraplp.Solver.CBC_MIXED_INTEGER_PROGRAMMING)

  #
  # data
  #
  # commodities
  num_commodities = 77
  C = list(range(num_commodities))

  #  days in a year
  days = 365.25

  # nutrients
  num_nutrients = 9
  N = list(range(num_nutrients))

  nutrients = [
      "calories",  # Calories, unit = 1000
      "protein",  # Protein, unit = grams
      "calcium",  # Calcium, unit = grams
      "iron",  # Iron, unit = milligrams
      "vitaminA",  # Vitamin A, unit = 1000 International Units
      "thiamine",  # Thiamine, Vit. B1, unit = milligrams
      "riboflavin",  # Riboflavin, Vit. B2, unit = milligrams
      "niacin",  # Niacin (Nicotinic Acid), unit = milligrams
      "ascorbicAcid"  # Ascorbic Acid, Vit. C, unit = milligrams
  ]

  commodities = [["Wheat Flour (Enriched)", "10 lb."], ["Macaroni", "1 lb."],
                 ["Wheat Cereal (Enriched)",
                  "28 oz."], ["Corn Flakes", "8 oz."], ["Corn Meal", "1 lb."],
                 ["Hominy Grits", "24 oz."], ["Rice", "1 lb."],
                 ["Rolled Oats", "1 lb."], ["White Bread (Enriched)", "1 lb."],
                 ["Whole Wheat Bread", "1 lb."], ["Rye Bread", "1 lb."],
                 ["Pound Cake", "1 lb."], ["Soda Crackers", "1 lb."],
                 ["Milk", "1 qt."], ["Evaporated Milk (can)", "14.5 oz."],
                 ["Butter", "1 lb."], ["Oleomargarine", "1 lb."],
                 ["Eggs", "1 doz."], ["Cheese (Cheddar)", "1 lb."],
                 ["Cream", "1/2 pt."], ["Peanut Butter", "1 lb."],
                 ["Mayonnaise", "1/2 pt."], ["Crisco", "1 lb."],
                 ["Lard", "1 lb."], ["Sirloin Steak", "1 lb."],
                 ["Round Steak", "1 lb."], ["Rib Roast", "1 lb."],
                 ["Chuck Roast", "1 lb."], ["Plate", "1 lb."],
                 ["Liver (Beef)", "1 lb."], ["Leg of Lamb", "1 lb."],
                 ["Lamb Chops (Rib)", "1 lb."], ["Pork Chops", "1 lb."],
                 ["Pork Loin Roast", "1 lb."], ["Bacon", "1 lb."],
                 ["Ham - smoked", "1 lb."], ["Salt Pork", "1 lb."],
                 ["Roasting Chicken", "1 lb."], ["Veal Cutlets", "1 lb."],
                 ["Salmon, Pink (can)", "16 oz."], ["Apples", "1 lb."],
                 ["Bananas", "1 lb."], ["Lemons", "1 doz."],
                 ["Oranges", "1 doz."], ["Green Beans", "1 lb."],
                 ["Cabbage", "1 lb."], ["Carrots", "1 bunch"],
                 ["Celery", "1 stalk"], ["Lettuce", "1 head"],
                 ["Onions", "1 lb."], ["Potatoes", "15 lb."],
                 ["Spinach", "1 lb."], ["Sweet Potatoes", "1 lb."],
                 ["Peaches (can)", "No. 2 1/2"], ["Pears (can)", "No. 2 1/2,"],
                 ["Pineapple (can)", "No. 2 1/2"], ["Asparagus (can)", "No. 2"],
                 ["Grean Beans (can)", "No. 2"],
                 ["Pork and Beans (can)", "16 oz."], ["Corn (can)", "No. 2"],
                 ["Peas (can)", "No. 2"], ["Tomatoes (can)", "No. 2"],
                 ["Tomato Soup (can)", "10 1/2 oz."],
                 ["Peaches, Dried", "1 lb."], ["Prunes, Dried", "1 lb."],
                 ["Raisins, Dried", "15 oz."], ["Peas, Dried", "1 lb."],
                 ["Lima Beans, Dried", "1 lb."], ["Navy Beans, Dried", "1 lb."],
                 ["Coffee", "1 lb."], ["Tea", "1/4 lb."], ["Cocoa", "8 oz."],
                 ["Chocolate", "8 oz."], ["Sugar", "10 lb."],
                 ["Corn Sirup", "24 oz."], ["Molasses", "18 oz."],
                 ["Strawberry Preserve", "1 lb."]]

  # price and weight are the two first columns
  data = [
      [36.0, 12600.0, 44.7, 1411.0, 2.0, 365.0, 0.0, 55.4, 33.3, 441.0, 0.0],
      [14.1, 3217.0, 11.6, 418.0, 0.7, 54.0, 0.0, 3.2, 1.9, 68.0, 0.0],
      [24.2, 3280.0, 11.8, 377.0, 14.4, 175.0, 0.0, 14.4, 8.8, 114.0, 0.0],
      [7.1, 3194.0, 11.4, 252.0, 0.1, 56.0, 0.0, 13.5, 2.3, 68.0, 0.0],
      [4.6, 9861.0, 36.0, 897.0, 1.7, 99.0, 30.9, 17.4, 7.9, 106.0, 0.0],
      [8.5, 8005.0, 28.6, 680.0, 0.8, 80.0, 0.0, 10.6, 1.6, 110.0, 0.0],
      [7.5, 6048.0, 21.2, 460.0, 0.6, 41.0, 0.0, 2.0, 4.8, 60.0, 0.0],
      [7.1, 6389.0, 25.3, 907.0, 5.1, 341.0, 0.0, 37.1, 8.9, 64.0, 0.0],
      [7.9, 5742.0, 15.6, 488.0, 2.5, 115.0, 0.0, 13.8, 8.5, 126.0, 0.0],
      [9.1, 4985.0, 12.2, 484.0, 2.7, 125.0, 0.0, 13.9, 6.4, 160.0, 0.0],
      [9.2, 4930.0, 12.4, 439.0, 1.1, 82.0, 0.0, 9.9, 3.0, 66.0, 0.0],
      [24.8, 1829.0, 8.0, 130.0, 0.4, 31.0, 18.9, 2.8, 3.0, 17.0, 0.0],
      [15.1, 3004.0, 12.5, 288.0, 0.5, 50.0, 0.0, 0.0, 0.0, 0.0, 0.0],
      [11.0, 8867.0, 6.1, 310.0, 10.5, 18.0, 16.8, 4.0, 16.0, 7.0, 177.0],
      [6.7, 6035.0, 8.4, 422.0, 15.1, 9.0, 26.0, 3.0, 23.5, 11.0, 60.0],
      [20.8, 1473.0, 10.8, 9.0, 0.2, 3.0, 44.2, 0.0, 0.2, 2.0, 0.0],
      [16.1, 2817.0, 20.6, 17.0, 0.6, 6.0, 55.8, 0.2, 0.0, 0.0, 0.0],
      [32.6, 1857.0, 2.9, 238.0, 1.0, 52.0, 18.6, 2.8, 6.5, 1.0, 0.0],
      [24.2, 1874.0, 7.4, 448.0, 16.4, 19.0, 28.1, 0.8, 10.3, 4.0, 0.0],
      [14.1, 1689.0, 3.5, 49.0, 1.7, 3.0, 16.9, 0.6, 2.5, 0.0, 17.0],
      [17.9, 2534.0, 15.7, 661.0, 1.0, 48.0, 0.0, 9.6, 8.1, 471.0, 0.0],
      [16.7, 1198.0, 8.6, 18.0, 0.2, 8.0, 2.7, 0.4, 0.5, 0.0, 0.0],
      [20.3, 2234.0, 20.1, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0],
      [9.8, 4628.0, 41.7, 0.0, 0.0, 0.0, 0.2, 0.0, 0.5, 5.0, 0.0],
      [39.6, 1145.0, 2.9, 166.0, 0.1, 34.0, 0.2, 2.1, 2.9, 69.0, 0.0],
      [36.4, 1246.0, 2.2, 214.0, 0.1, 32.0, 0.4, 2.5, 2.4, 87.0, 0.0],
      [29.2, 1553.0, 3.4, 213.0, 0.1, 33.0, 0.0, 0.0, 2.0, 0.0, 0.0],
      [22.6, 2007.0, 3.6, 309.0, 0.2, 46.0, 0.4, 1.0, 4.0, 120.0, 0.0],
      [14.6, 3107.0, 8.5, 404.0, 0.2, 62.0, 0.0, 0.9, 0.0, 0.0, 0.0],
      [26.8, 1692.0, 2.2, 333.0, 0.2, 139.0, 169.2, 6.4, 50.8, 316.0, 525.0],
      [27.6, 1643.0, 3.1, 245.0, 0.1, 20.0, 0.0, 2.8, 3.0, 86.0, 0.0],
      [36.6, 1239.0, 3.3, 140.0, 0.1, 15.0, 0.0, 1.7, 2.7, 54.0, 0.0],
      [30.7, 1477.0, 3.5, 196.0, 0.2, 80.0, 0.0, 17.4, 2.7, 60.0, 0.0],
      [24.2, 1874.0, 4.4, 249.0, 0.3, 37.0, 0.0, 18.2, 3.6, 79.0, 0.0],
      [25.6, 1772.0, 10.4, 152.0, 0.2, 23.0, 0.0, 1.8, 1.8, 71.0, 0.0],
      [27.4, 1655.0, 6.7, 212.0, 0.2, 31.0, 0.0, 9.9, 3.3, 50.0, 0.0],
      [16.0, 2835.0, 18.8, 164.0, 0.1, 26.0, 0.0, 1.4, 1.8, 0.0, 0.0],
      [30.3, 1497.0, 1.8, 184.0, 0.1, 30.0, 0.1, 0.9, 1.8, 68.0, 46.0],
      [42.3, 1072.0, 1.7, 156.0, 0.1, 24.0, 0.0, 1.4, 2.4, 57.0, 0.0],
      [13.0, 3489.0, 5.8, 705.0, 6.8, 45.0, 3.5, 1.0, 4.9, 209.0, 0.0],
      [4.4, 9072.0, 5.8, 27.0, 0.5, 36.0, 7.3, 3.6, 2.7, 5.0, 544.0],
      [6.1, 4982.0, 4.9, 60.0, 0.4, 30.0, 17.4, 2.5, 3.5, 28.0, 498.0],
      [26.0, 2380.0, 1.0, 21.0, 0.5, 14.0, 0.0, 0.5, 0.0, 4.0, 952.0],
      [30.9, 4439.0, 2.2, 40.0, 1.1, 18.0, 11.1, 3.6, 1.3, 10.0, 1993.0],
      [7.1, 5750.0, 2.4, 138.0, 3.7, 80.0, 69.0, 4.3, 5.8, 37.0, 862.0],
      [3.7, 8949.0, 2.6, 125.0, 4.0, 36.0, 7.2, 9.0, 4.5, 26.0, 5369.0],
      [4.7, 6080.0, 2.7, 73.0, 2.8, 43.0, 188.5, 6.1, 4.3, 89.0, 608.0],
      [7.3, 3915.0, 0.9, 51.0, 3.0, 23.0, 0.9, 1.4, 1.4, 9.0, 313.0],
      [8.2, 2247.0, 0.4, 27.0, 1.1, 22.0, 112.4, 1.8, 3.4, 11.0, 449.0],
      [3.6, 11844.0, 5.8, 166.0, 3.8, 59.0, 16.6, 4.7, 5.9, 21.0, 1184.0],
      [34.0, 16810.0, 14.3, 336.0, 1.8, 118.0, 6.7, 29.4, 7.1, 198.0, 2522.0],
      [8.1, 4592.0, 1.1, 106.0, 0.0, 138.0, 918.4, 5.7, 13.8, 33.0, 2755.0],
      [5.1, 7649.0, 9.6, 138.0, 2.7, 54.0, 290.7, 8.4, 5.4, 83.0, 1912.0],
      [16.8, 4894.0, 3.7, 20.0, 0.4, 10.0, 21.5, 0.5, 1.0, 31.0, 196.0],
      [20.4, 4030.0, 3.0, 8.0, 0.3, 8.0, 0.8, 0.8, 0.8, 5.0, 81.0],
      [21.3, 3993.0, 2.4, 16.0, 0.4, 8.0, 2.0, 2.8, 0.8, 7.0, 399.0],
      [27.7, 1945.0, 0.4, 33.0, 0.3, 12.0, 16.3, 1.4, 2.1, 17.0, 272.0],
      [10.0, 5386.0, 1.0, 54.0, 2.0, 65.0, 53.9, 1.6, 4.3, 32.0, 431.0],
      [7.1, 6389.0, 7.5, 364.0, 4.0, 134.0, 3.5, 8.3, 7.7, 56.0, 0.0],
      [10.4, 5452.0, 5.2, 136.0, 0.2, 16.0, 12.0, 1.6, 2.7, 42.0, 218.0],
      [13.8, 4109.0, 2.3, 136.0, 0.6, 45.0, 34.9, 4.9, 2.5, 37.0, 370.0],
      [8.6, 6263.0, 1.3, 63.0, 0.7, 38.0, 53.2, 3.4, 2.5, 36.0, 1253.0],
      [7.6, 3917.0, 1.6, 71.0, 0.6, 43.0, 57.9, 3.5, 2.4, 67.0, 862.0],
      [15.7, 2889.0, 8.5, 87.0, 1.7, 173.0, 86.8, 1.2, 4.3, 55.0, 57.0],
      [9.0, 4284.0, 12.8, 99.0, 2.5, 154.0, 85.7, 3.9, 4.3, 65.0, 257.0],
      [9.4, 4524.0, 13.5, 104.0, 2.5, 136.0, 4.5, 6.3, 1.4, 24.0, 136.0],
      [7.9, 5742.0, 20.0, 1367.0, 4.2, 345.0, 2.9, 28.7, 18.4, 162.0, 0.0],
      [8.9, 5097.0, 17.4, 1055.0, 3.7, 459.0, 5.1, 26.9, 38.2, 93.0, 0.0],
      [5.9, 7688.0, 26.9, 1691.0, 11.4, 792.0, 0.0, 38.4, 24.6, 217.0, 0.0],
      [22.4, 2025.0, 0.0, 0.0, 0.0, 0.0, 0.0, 4.0, 5.1, 50.0, 0.0],
      [17.4, 652.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 2.3, 42.0, 0.0],
      [8.6, 2637.0, 8.7, 237.0, 3.0, 72.0, 0.0, 2.0, 11.9, 40.0, 0.0],
      [16.2, 1400.0, 8.0, 77.0, 1.3, 39.0, 0.0, 0.9, 3.4, 14.0, 0.0],
      [51.7, 8773.0, 34.9, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0],
      [13.7, 4996.0, 14.7, 0.0, 0.5, 74.0, 0.0, 0.0, 0.0, 5.0, 0.0],
      [13.6, 3752.0, 9.0, 0.0, 10.3, 244.0, 0.0, 1.9, 7.5, 146.0, 0.0],
      [20.5, 2213.0, 6.4, 11.0, 0.4, 7.0, 0.2, 0.2, 0.4, 3.0, 0.0]
  ]

  # recommended daily allowance for a moderately active man
  allowance = [3.0, 70.0, 0.8, 12.0, 5.0, 1.8, 2.7, 18.0, 75.0]

  #
  # variables
  #
  x = [solver.NumVar(0, 1000, "x[%i]" % i) for i in C]
  x_cost = [solver.NumVar(0, 1000, "x_cost[%i]" % i) for i in C]
  quant = [solver.NumVar(0, 1000, "quant[%i]" % i) for i in C]

  # total food bill
  total_cost = solver.NumVar(0, 1000, "total_cost")

  # cost per day, to minimize
  cost = solver.Sum(x)

  #
  # constraints
  #
  solver.Add(total_cost == days * cost)  # cost per year

  for c in C:
    solver.Add(x_cost[c] == days * x[c])
    solver.Add(quant[c] == 100.0 * days * x[c] / data[c][0])

  # nutrient balance
  for n in range(2, num_nutrients + 2):
    solver.Add(solver.Sum([data[c][n] * x[c] for c in C]) >= allowance[n - 2])

  objective = solver.Minimize(cost)

  #
  # solution and search
  #
  solver.Solve()

  print()

  print("Cost = %0.2f" % solver.Objective().Value())
  # print 'Cost:', cost.SolutionValue()
  print("Total cost: %0.2f" % total_cost.SolutionValue())
  print()
  for i in C:
    if x[i].SolutionValue() > 0:
      print("%-21s %-11s  %0.2f  %0.2f" %
            (commodities[i][0], commodities[i][1], x_cost[i].SolutionValue(),
             quant[i].SolutionValue()))

  print()

  print("walltime  :", solver.WallTime(), "ms")
  if sol == "CBC":
    print("iterations:", solver.Iterations())


if __name__ == "__main__":
  sol = "CBC"

  if len(sys.argv) > 1:
    sol = sys.argv[1]
    if sol != "GLPK" and sol != "CBC":
      print("Solver must be either GLPK or CBC")
      sys.exit(1)

  main(sol)
