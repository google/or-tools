# Copyright 2018 Gergo Rozner
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http:#www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
"""Solves the stable marriage problem in CP-SAT."""

from ortools.sat.python import cp_model


class SolutionPrinter(cp_model.CpSolverSolutionCallback):

  def __init__(self, wife, husband):
    cp_model.CpSolverSolutionCallback.__init__(self)
    self.__wife = wife
    self.__husband = husband
    self.__solution_count = 0
    self.__n = len(wife)

  def OnSolutionCallback(self):
    self.__solution_count += 1
    print("Solution %i" % self.__solution_count)
    print("wife   : ", [self.Value(self.__wife[i]) for i in range(self.__n)])
    print("husband: ", [self.Value(self.__husband[i]) for i in range(self.__n)])
    print()

  def SolutionCount(self):
    return self.__solution_count


def main(ranks, pair_num):
  mrank = ranks["rankMen"]
  wrank = ranks["rankWomen"]

  n = pair_num

  model = cp_model.CpModel()

  wife = [model.NewIntVar(0, n - 1, "wife[%i]" % i) for i in range(n)]
  husband = [model.NewIntVar(0, n - 1, "husband[%i]" % i) for i in range(n)]

  for m in range(n):
    model.AddElement(wife[m], husband, m)

  for w in range(n):
    model.AddElement(husband[w], wife, w)

  #mrank[w][m] < mrank[w][husband[w]] => wrank[m][wife[m]] < wrank[m][w]
  for w in range(n):
    for m in range(n):
      husband_rank = model.NewIntVar(1, n, "")
      model.AddElement(husband[w], mrank[w], husband_rank)

      wife_rank = model.NewIntVar(1, n, "")
      model.AddElement(wife[m], wrank[m], wife_rank)

      husband_dominated = model.NewBoolVar("")
      model.Add(mrank[w][m] < husband_rank).OnlyEnforceIf(husband_dominated)
      model.Add(mrank[w][m] >= husband_rank).OnlyEnforceIf(
          husband_dominated.Not())

      wife_dominates = model.NewBoolVar("")
      model.Add(wife_rank < wrank[m][w]).OnlyEnforceIf(wife_dominates)
      model.Add(wife_rank >= wrank[m][w]).OnlyEnforceIf(wife_dominates.Not())

      model.AddImplication(husband_dominated, wife_dominates)

  #wrank[m][w] < wrank[m][wife[m]] => mrank[w][husband[w]] < mrank[w][m]
  for m in range(n):
    for w in range(n):
      wife_rank = model.NewIntVar(1, n, "")
      model.AddElement(wife[m], wrank[m], wife_rank)

      husband_rank = model.NewIntVar(1, n, "")
      model.AddElement(husband[w], mrank[w], husband_rank)

      wife_dominated = model.NewBoolVar("")
      model.Add(wrank[m][w] < wife_rank).OnlyEnforceIf(wife_dominated)
      model.Add(wrank[m][w] >= wife_rank).OnlyEnforceIf(wife_dominated.Not())

      husband_dominates = model.NewBoolVar("")
      model.Add(husband_rank < mrank[w][m]).OnlyEnforceIf(husband_dominates)
      model.Add(husband_rank >= mrank[w][m]).OnlyEnforceIf(
          husband_dominates.Not())

      model.AddImplication(wife_dominated, husband_dominates)

  solver = cp_model.CpSolver()
  solution_printer = SolutionPrinter(wife, husband)
  solver.parameters.enumerate_all_solutions = True
  solver.Solve(model, solution_printer)


if __name__ == "__main__":
  rankings1 = {
      "rankMen": [[1, 2, 3, 4], [4, 3, 2, 1], [1, 2, 3, 4], [3, 4, 1, 2]],
      "rankWomen": [[1, 2, 3, 4], [2, 1, 3, 4], [1, 4, 3, 2], [4, 3, 1, 2]]
  }

  rankings2 = {
      "rankMen": [[1, 5, 4, 6, 2, 3], [4, 1, 5, 2, 6, 3], [6, 4, 2, 1, 5, 3],
                  [1, 5, 2, 4, 3, 6], [4, 2, 1, 5, 6, 3], [2, 6, 3, 5, 1, 4]],
      "rankWomen": [[1, 4, 2, 5, 6, 3], [3, 4, 6, 1, 5, 2], [1, 6, 4, 2, 3, 5],
                    [6, 5, 3, 4, 2, 1], [3, 1, 2, 4, 5, 6], [2, 3, 1, 6, 5, 4]]
  }

  problem = rankings2
  couple_count = len(problem["rankMen"])

  main(problem, couple_count)
