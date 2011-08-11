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


class PyWrapLPExamples(object):
  """Class that contains a collection of LP examples."""

  def RunFirstLinearExample(self, mode):
    """Minimal Linear Example."""
    solver = pywraplp.Solver('RunFirstLinearExample', mode)
    infinity = solver.infinity()
    x1 = solver.NumVar(0.0, infinity, 'x1')
    x2 = solver.NumVar(0.0, infinity, 'x2')
    x3 = solver.NumVar(0.0, infinity, 'x3')
    print 'number of variables = ', solver.NumVariables()

    solver.AddObjectiveTerm(x1, 10)
    solver.AddObjectiveTerm(x2, 6)
    solver.AddObjectiveTerm(x3, 4)
    solver.SetMaximization()

    c0 = solver.Constraint(-infinity, 100.0)
    c0.AddTerm(x1, 1)
    c0.AddTerm(x2, 1)
    c0.AddTerm(x3, 1)
    c1 = solver.Constraint(-infinity, 600.0)
    c1.AddTerm(x1, 10)
    c1.AddTerm(x2, 4)
    c1.AddTerm(x3, 5)
    c2 = solver.Constraint(-infinity, 300.0)
    c2.AddTerm(x1, 2)
    c2.AddTerm(x2, 2)
    c2.AddTerm(x3, 6)
    print 'number of constraints = ', solver.NumConstraints()

    # The problem has an optimal solution.
    solver.Solve()

    print 'optimal objective value = ', solver.objective_value()
    print 'x1 = ', x1.solution_value(), ', reduced_cost = ', x1.reduced_cost()
    print 'x2 = ', x2.solution_value(), ', reduced_cost = ', x2.reduced_cost()
    print 'x3 = ', x3.solution_value(), ', reduced_cost = ', x3.reduced_cost()
    print 'c0 dual value = ', c0.dual_value()
    print 'c0 activity = ', c0.activity()
    print 'c1 dual value = ', c1.dual_value()
    print 'c1 activity = ', c1.activity()
    print 'c2 dual value = ', c2.dual_value()
    print 'c2 activity = ', c2.activity()

  def RunAllFirstLinearExample(self):
    self.RunFirstLinearExample(pywraplp.Solver.GLPK_LINEAR_PROGRAMMING)
    self.RunFirstLinearExample(pywraplp.Solver.CLP_LINEAR_PROGRAMMING)

  def RunFirstLinearExampleNewAPI(self, mode):
    """Minimal LP Example with New API."""
    solver = pywraplp.Solver('RunFirstLinearExample', mode)
    infinity = solver.infinity()
    x1 = solver.NumVar(0.0, infinity, 'x1')
    x2 = solver.NumVar(0.0, infinity, 'x2')
    x3 = solver.NumVar(0.0, infinity, 'x3')
    print 'number of variables = ', solver.NumVariables()

    solver.Maximize(10 * x1 + 6 * x2 + 4 * x3)

    c0 = solver.Add(x1 + x2 + x3 <= 100.0)
    c1 = solver.Add(10 * x1 + 4 * x2 + 5 * x3 <= 600)
    c2 = solver.Add(2 * x1 + 2 * x2 + 6 * x3 <= 300)

    print 'number of constraints = ', solver.NumConstraints()

    # The problem has an optimal solution.
    solver.Solve()

    print 'optimal objective value = ', solver.objective_value()
    print 'x1 = ', x1.solution_value(), ', reduced_cost = ', x1.reduced_cost()
    print 'x2 = ', x2.solution_value(), ', reduced_cost = ', x2.reduced_cost()
    print 'x3 = ', x3.solution_value(), ', reduced_cost = ', x3.reduced_cost()
    print 'c0 dual value = ', c0.dual_value()
    print 'c0 activity = ', c0.activity()
    print 'c1 dual value = ', c1.dual_value()
    print 'c1 activity = ', c1.activity()
    print 'c2 dual value = ', c2.dual_value()
    print 'c2 activity = ', c2.activity()

  def RunAllFirstLinearExampleNewAPI(self):
    self.RunFirstLinearExampleNewAPI(pywraplp.Solver.GLPK_LINEAR_PROGRAMMING)
    self.RunFirstLinearExampleNewAPI(pywraplp.Solver.CLP_LINEAR_PROGRAMMING)

  def RunSuccessiveObjectives(self, mode):
    """Example with succesive objectives."""
    solver = pywraplp.Solver('RunSuccessiveObjectives', mode)
    x1 = solver.NumVar(0, 10, 'var1')
    x2 = solver.NumVar(0, 10, 'var2')
    solver.Add(x1 + 2*x2 <= 10)
    solver.Maximize(x1)

    # Check the solution
    solver.Solve()
    print 'x1 = ', x1.solution_value()
    print 'x2 = ', x2.solution_value()

    solver.Maximize(x2)

    # Check the solution
    solver.Solve()
    print 'x1 = ', x1.solution_value()
    print 'x2 = ', x2.solution_value()

    solver.Minimize(-x1)

    # Check the solution
    solver.Solve()
    print 'x1 = ', x1.solution_value()
    print 'x2 = ', x2.solution_value()

  def RunAllSuccessiveObjectives(self):
    self.RunSuccessiveObjectives(pywraplp.Solver.GLPK_LINEAR_PROGRAMMING)
    self.RunSuccessiveObjectives(pywraplp.Solver.CLP_LINEAR_PROGRAMMING)

  def RunAllExamples(self):
    self.RunAllFirstLinearExample()
    self.RunAllFirstLinearExampleNewAPI()
    self.RunAllSuccessiveObjectives()


def main(unused_argv):
  lp_example = PyWrapLPExamples()
  lp_example.RunAllExamples()


if __name__ == '__main__':
  app.run()
