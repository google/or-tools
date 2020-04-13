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
"""Solve the magic sequence problem with the CP-SAT solver."""


from ortools.sat.python import cp_model


def main():
  """Magic sequence problem."""
  n = 100
  values = range(n)

  model = cp_model.CpModel()

  x = [model.NewIntVar(0, n, 'x%i' % i) for i in values]

  for k in values:
    tmp_array = []
    for i in values:
      tmp_var = model.NewBoolVar('')
      model.Add(x[i] == k).OnlyEnforceIf(tmp_var)
      model.Add(x[i] != k).OnlyEnforceIf(tmp_var.Not())
      tmp_array.append(tmp_var)
    model.Add(sum(tmp_array) == x[k])

  # Redundant constraint.
  model.Add(sum(x) == n)

  solver = cp_model.CpSolver()
  # No solution printer, this problem has only 1 solution.
  solver.parameters.log_search_progress = True
  solver.Solve(model)
  print(solver.ResponseStats())
  for k in values:
    print('x[%i] = %i ' % (k, solver.Value(x[k])), end='')
  print()


if __name__ == '__main__':
  main()
