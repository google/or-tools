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

"""Code for issue 46 in or-tools."""

from ortools.constraint_solver import pywrapcp

class AssignToStartMin(pywrapcp.PyDecisionBuilder):

  def __init__(self, intervals):
    self.__intervals = intervals

  def Next(self, solver):
    for interval in self.__intervals:
      interval.SetStartMax(interval.StartMin())
    return None

def NoSequence():
    print 'NoSequence'
    solver = pywrapcp.Solver('Ordo')
    tasks = []
    [tasks.append(solver.FixedDurationIntervalVar(0, 25, 5,
False, 'Tasks%i' %i)) for i in range(3)]
    print tasks
    disj = solver.DisjunctiveConstraint(tasks, 'Disjunctive')
    solver.Add(disj)
    collector = solver.AllSolutionCollector()
    collector.Add(tasks)
    intervalPhase = solver.Phase(tasks, solver.INTERVAL_DEFAULT)
    solver.Solve(intervalPhase, [ collector])
    print collector.SolutionCount()
    for i in range(collector.SolutionCount()):
        print "Solution " , i
        print collector.ObjectiveValue(i)
        print [collector.StartValue(i, tasks[j]) for j in range(3)]
        print [collector.EndValue(i, tasks[j]) for j in range(3)]

def Sequence():
    print 'Sequence'
    solver = pywrapcp.Solver('Ordo')
    tasks = []
    [tasks.append(solver.FixedDurationIntervalVar(0, 25, 5, False,
                                                  'Tasks%i' %i))
     for i in range(3)]
    print tasks
    disj = solver.DisjunctiveConstraint(tasks, 'Disjunctive')
    solver.Add(disj)
    sequence = []
    sequence.append(disj.SequenceVar())
    sequence[0].RankFirst(0)
    collector = solver.AllSolutionCollector()
    collector.Add(sequence)
    collector.Add(tasks)
    sequencePhase = solver.Phase(sequence, solver.SEQUENCE_DEFAULT)
    intervalPhase = AssignToStartMin(tasks)
    # intervalPhase = solver.Phase(tasks, solver.INTERVAL_DEFAULT)
    mainPhase = solver.Compose([sequencePhase, intervalPhase])
    solver.Solve(mainPhase, [ collector])
    print collector.SolutionCount()
    for i in range(collector.SolutionCount()):
        print "Solution " , i
        print collector.ObjectiveValue(i)
        print [collector.StartValue(i, tasks[j]) for j in range(3)]
        print [collector.EndValue(i, tasks[j]) for j in range(3)]


def main():
    NoSequence()
    Sequence()


if __name__ == '__main__':
  main()
