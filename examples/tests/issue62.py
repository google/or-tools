from ortools.constraint_solver import pywrapcp


class PushLeft(pywrapcp.PyDecisionBuilder):

  def __init__(self, vars):
    pywrapcp.PyDecisionBuilder.__init__(self)
    self.__vars = vars

  def Next(self, Solver):
    for v in self.__vars:
      v.SetStartMax(v.StartMin())
    return None

  def DebugString():
    return 'PushLeft'


def main():
    solver = pywrapcp.Solver('Ordo')
    tasks = [solver.FixedDurationIntervalVar(0, 25, 5, False, 'Tasks%i' %i)
             for i in range(3)]
    print tasks
    disj = solver.DisjunctiveConstraint(tasks, 'Disjunctive')
    sequence = []
    sequence.append(disj.SequenceVar())
    solver.Add(disj)
    collector = solver.AllSolutionCollector()
    collector.Add(sequence)
    collector.Add(tasks)
    sequencePhase = solver.Phase(sequence, solver.SEQUENCE_DEFAULT)
    intervalPhase = PushLeft(tasks)
    mainPhase = solver.Compose([sequencePhase, intervalPhase])
    solver.Solve(mainPhase, [ collector])
    print collector.SolutionCount()
    for i in range(collector.SolutionCount()):
        print "Solution " , i
        print [collector.StartValue(i, tasks[j]) for j in range(3)]
        print [collector.EndValue(i, tasks[j]) for j in range(3)]


if __name__ == '__main__':
  main()
