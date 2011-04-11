# Copyright 2010-2011 Google
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

"""pywraplp example file."""



from google.apputils import app
import gflags

from linear_solver import pywraplp

FLAGS = gflags.FLAGS


class PyWrapMIPExamples(object):
  """Class that contains a collection of MIP examples."""

  def RunFirstMIPExample(self, mode):
    """Minimal MIP Example."""
    solver = pywraplp.Solver('RunFirstMIPExample', mode)
    infinity = solver.infinity()
    x1 = solver.IntVar(0.0, infinity, 'x1')
    x2 = solver.IntVar(0.0, infinity, 'x2')

    solver.AddObjectiveTerm(x1, 1)
    solver.AddObjectiveTerm(x2, 2)

    ct = solver.Constraint(17, infinity)
    ct.AddTerm(x1, 3)
    ct.AddTerm(x2, 2)

    # Check the solution.
    solver.Solve()
    print solver.objective_value()

  def RunAllFirstMIPExample(self):
    self.RunFirstMIPExample(pywraplp.Solver.GLPK_MIXED_INTEGER_PROGRAMMING)
    self.RunFirstMIPExample(pywraplp.Solver.CBC_MIXED_INTEGER_PROGRAMMING)

  def RunFirstMIPExampleNewAPI(self, mode):
    """Minimal MIP Example with New API."""
    solver = pywraplp.Solver('RunFirstMIPExample', mode)
    infinity = solver.infinity()
    x1 = solver.IntVar(0.0, infinity, 'x1')
    x2 = solver.IntVar(0.0, infinity, 'x2')

    solver.Minimize(x1 + 2 * x2)
    solver.Add(3 * x1 + 2 * x2 >= 17)

    # Check the solution.
    solver.Solve()
    print solver.objective_value()

  def RunAllFirstMIPExampleNewAPI(self):
    self.RunFirstMIPExampleNewAPI(
        pywraplp.Solver.GLPK_MIXED_INTEGER_PROGRAMMING)
    self.RunFirstMIPExampleNewAPI(pywraplp.Solver.CBC_MIXED_INTEGER_PROGRAMMING)

  def RunSuccessiveObjectives(self, mode):
    """Example with succesive objectives."""
    solver = pywraplp.Solver('RunSuccessiveObjectives', mode)
    x1 = solver.NumVar(0, 10, 'var1')
    x2 = solver.NumVar(0, 10, 'var2')
    solver.Add(x1 + 2*x2 <= 10)
    solver.Maximize(x1)

    # Check the solution
    solver.Solve()
    print x1.solution_value()
    print x2.solution_value()

    solver.Maximize(x2)

    # Check the solution
    solver.Solve()
    print x1.solution_value()
    print x2.solution_value()

    solver.Minimize(-x1)

    # Check the solution
    solver.Solve()
    print x1.solution_value()
    print x2.solution_value()

  def RunAllSuccessiveObjectives(self):
    self.RunSuccessiveObjectives(pywraplp.Solver.GLPK_MIXED_INTEGER_PROGRAMMING)
    self.RunSuccessiveObjectives(pywraplp.Solver.CBC_MIXED_INTEGER_PROGRAMMING)

  def RunAllExamples(self):
    self.RunAllFirstMIPExample()
    self.RunAllFirstMIPExampleNewAPI()
    self.RunAllSuccessiveObjectives()


def main(unused_argv):
  mip_example = PyWrapMIPExamples()
  mip_example.RunAllExamples()


if __name__ == '__main__':
  app.run()
