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

from __future__ import print_function

from ortools.sat.python import cp_model


model = cp_model.CpModel()

boo_x_i_j = {}
for i in range(100):
    for j in range(10):
        boo_x_i_j[(i, j)] = model.NewBoolVar('x%d belongs to group %d' % (i, j))

e = model.NewIntVar(0, 5, 'epsilon')
values = [i + 1 for i in range(100)]
sum_of_values = sum(values)
average_value = sum_of_values / 10

for j in range(10):
    model.Add(sum(boo_x_i_j[(i, j)] for i in range(100)) == 10)

for i in range(100):
    model.Add(sum(boo_x_i_j[(i, j)] for j in range(10)) == 1)

for j in range(10):
    model.Add(-e <= sum(boo_x_i_j[(i, j)]*values[i] for i in range(100)) -
              average_value <= e)
model.Minimize(e)

solver = cp_model.CpSolver()
status = solver.Solve(model)
print('Optimal epsilon: %i' % solver.ObjectiveValue())
print('Statistics')
print('  - conflicts : %i' % solver.NumConflicts())
print('  - branches  : %i' % solver.NumBranches())
print('  - wall time : %f s' % solver.WallTime())

groups = {}
for j in range(10):
    groups[j] = []
for i in range(100):
    for j in range(10):
        if solver.Value(boo_x_i_j[(i, j)]):
            groups[j].append(values[i])

for j in range(10):
    print ('group %i: average = %0.2f [' % (j, sum(groups[j]) / 10.0), end='')
    for v in groups[j]:
        print(' %i' % v, end='')
    print(' ]')
