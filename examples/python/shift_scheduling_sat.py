#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
"""Creates a shift scheduling problem and solves it."""

from __future__ import print_function

import time

from ortools.sat.python import cp_model


class ObjectiveSolutionPrinter(cp_model.CpSolverSolutionCallback):
    """Print intermediate solutions objective and time."""

    def __init__(self):
        cp_model.CpSolverSolutionCallback.__init__(self)
        self.__solution_count = 0
        self.__start_time = time.time()

    def OnSolutionCallback(self):
        """Called on each new solution."""
        current_time = time.time()
        objective = self.ObjectiveValue()
        print('Solution %i, time = %f s, objective = [%i, %i]' %
              (self.__solution_count, current_time - self.__start_time,
               objective, self.BestObjectiveBound()))
        self.__solution_count += 1

    def SolutionCount(self):
        return self.__solution_count


def negated_span_with_borders(works, start, length):
    """Extract a sub-sequence of works with the given length, with borders."""
    sequence = []
    pos = start
    if pos != 0:
        sequence.append(works[pos])
        pos += 1
    for _ in range(length):
        sequence.append(works[pos].Not())
        pos += 1
    if pos < len(works):
        sequence.append(works[pos])
    return sequence


def add_sequence_constraint(model, works, hard_min, soft_min, min_cost,
                            soft_max, hard_max, max_cost):
    """Sequence constraint with soft and hard bounds.
    
    This constraint look at every maximal contiguous sequence of variables
    assigned to true. If forbids sequence of length < hard_min or > hard_max.
    Then it creates penalty terms if the length is < soft_min or > soft_max.
    """
    cost_literals = []
    cost_coefficients = []
    for length in range(1, hard_min):
        for start in range(len(works) - length - 1):
            model.AddBoolOr(negated_span_with_borders(works, start, length))

    for length in range(hard_min, soft_min):
        for start in range(len(works) - length - 1):
            span = negated_span_with_borders(works, start, length)
            lit = model.NewBoolVar('')
            span.append(lit)
            model.AddBoolOr(span)
            cost_literals.append(lit)
            cost_coefficients.append(min_cost)

    for length in range(soft_max + 1, hard_max + 1):
        for start in range(len(works) - length - 1):
            span = negated_span_with_borders(works, start, length)
            lit = model.NewBoolVar('')
            span.append(lit)
            model.AddBoolOr(span)
            cost_literals.append(lit)
            cost_coefficients.append(max_cost)

    # Just forbid any sequence of true variables with length hard_max + 1
    for start in range(len(works) - hard_max - 1):
        model.AddBoolOr(
            [works[i].Not() for i in range(start, start + hard_max + 1)])
    return cost_literals, cost_coefficients


def add_weekly_sum_constraint(model, works, hard_min, soft_min, min_cost,
                              soft_max, hard_max, max_cost):
    """Sum constraint with soft and hard bounds.

    This constraint counts the variables assigned to true from works.
    If forbids sum < hard_min or > hard_max.
    Then it creates penalty terms if the sum is < soft_min or > soft_max.
    """
    cost_variables = []
    cost_coefficients = []
    sum_var = model.NewIntVar(hard_min, hard_max, '')
    model.Add(sum_var == sum(works))

    if soft_min > hard_min and min_cost > 0:
        delta = model.NewIntVar(-7, 7, '')
        model.Add(delta == soft_min - sum_var)
        excess = model.NewIntVar(0, 7, '')
        model.AddMaxEquality(excess, [delta, 0])
        cost_variables.append(excess)
        cost_coefficients.append(min_cost)

    if soft_max < hard_max and max_cost > 0:
        delta = model.NewIntVar(-7, 7, '')
        model.Add(delta == sum_var - soft_max)
        excess = model.NewIntVar(0, 7, '')
        model.AddMaxEquality(excess, [delta, 0])
        cost_variables.append(excess)
        cost_coefficients.append(max_cost)
    return cost_variables, cost_coefficients


def main():
    """Solves the shift scheduling problem."""
    # Data
    num_employees = 8
    num_weeks = 3
    shifts = ['O', 'M', 'A', 'N']

    # Fixed assignment: (employee, day, shift).
    # This fixes the first 2 days of the schedule.
    fixed_assignments = [
        (0, 0, 0),
        (1, 0, 0),
        (2, 0, 1),
        (3, 0, 1),
        (4, 0, 2),
        (5, 0, 2),
        (6, 0, 2),
        (7, 0, 3),
        (0, 1, 1),
        (1, 1, 1),
        (2, 1, 2),
        (3, 1, 2),
        (4, 1, 2),
        (5, 1, 0),
        (6, 1, 0),
        (7, 1, 3),
    ]

    # Request: (employee, day, shift, weight)
    positive_requests = [
        # Employee 3 wants the first Saturday off.
        (3, 5, 0, 2),
        # Employee 5 wants a night shift on the second Thursday.
        (5, 10, 3, 2)
    ]

    negative_requests = [
        # Employee 2 does not want a night shift on the third Friday.
        (2, 4, 3, 4)
    ]

    # Shift constraints on continuous sequence :
    #     (shift , hard_min, soft_min, min_penalty,
    #              soft_max, hard_max, max_penalty)
    shift_constraints = [
        # One or two consecutive days of rest, this is a hard constraint.
        (0, 1, 1, 0, 2, 2, 0),
        # betweem 2 and 3 consecutive days of night shifts, 1 and 4 are
        # possible but penalized.
        (3, 1, 2, 20, 3, 4, 5),
    ]

    # Weekly sum constraints on shifts days:
    #     (shift, hard_min, soft_min, min_penalty,
    #             soft_max, hard_max, max_penalty)
    weekly_sum_constraints = [
        # Constraints on rests per week.
        (0, 1, 2, 7, 2, 3, 4),
        # At least 1 night shift per week (penalized). At most 4 (hard).
        (3, 0, 1, 3, 4, 4, 0),
    ]

    # Penalized transitions:
    #     (previous_shift, next_shift, penalty (0 means forbidden))
    penalized_transitions = [
        # Afternoon to night has a penalty of 4.
        (2, 3, 4),
        # Night to morning is forbidden.
        (3, 1, 0),
    ]

    # daily demands for work shifts (morning, afternon, night) for each day
    # of the week starting on Monday.
    weekly_cover_demands = [
        (2, 3, 1),  # Monday
        (2, 3, 1),  # Tuesday
        (2, 2, 2),  # Wednesday
        (2, 3, 1),  # Thursday
        (2, 2, 2),  # Friday
        (1, 2, 3),  # Saturday
        (1, 3, 1),  # Sunday
    ]

    # Penalty for exceeding the cover constraint per shift type.
    excess_cover_penalties = (2, 2, 5)

    num_days = num_weeks * 7
    num_shifts = len(shifts)

    # Create model.
    model = cp_model.CpModel()

    work = {}
    for e in range(num_employees):
        for s in range(num_shifts):
            for d in range(num_days):
                work[e, s, d] = model.NewBoolVar('work%i_%i_%i' % (e, s, d))

    # Linear terms of the objective in a minimization context.
    obj_vars = []
    obj_coeffs = []

    # Exactly one shift per day.
    for e in range(num_employees):
        for d in range(num_days):
            model.Add(sum(work[e, s, d] for s in range(num_shifts)) == 1)

    # Fixed assignments.
    for e, d, s in fixed_assignments:
        model.Add(work[e, s, d] == 1)

    # Employee requests
    for e, d, s, w in positive_requests:
        obj_vars.append(work[e, s, d])
        obj_coeffs.append(-w)  # We gain 'w' is the shift is selected.

    for e, d, s, w in negative_requests:
        obj_vars.append(work[e, s, d])
        obj_coeffs.append(w)  # We loose 'w' is the shift is selected.

    # Shift constraints
    for ct in shift_constraints:
        shift, hard_min, soft_min, min_cost, soft_max, hard_max, max_cost = ct
        for e in range(num_employees):
            works = [work[e, shift, d] for d in range(num_days)]
            variables, coeffs = add_sequence_constraint(
                model, works, hard_min, soft_min, min_cost, soft_max, hard_max,
                max_cost)
            obj_vars.extend(variables)
            obj_coeffs.extend(coeffs)

    # Weekly sum constraints
    for ct in weekly_sum_constraints:
        shift, hard_min, soft_min, min_cost, soft_max, hard_max, max_cost = ct
        for e in range(num_employees):
            for w in range(num_weeks):
                works = [work[e, shift, d + w * 7] for d in range(7)]
                variables, coeffs = add_weekly_sum_constraint(
                    model, works, hard_min, soft_min, min_cost, soft_max,
                    hard_max, max_cost)
                obj_vars.extend(variables)
                obj_coeffs.extend(coeffs)

    # Penalized transitions
    for previous_shift, next_shift, cost in penalized_transitions:
        for e in range(num_employees):
            for d in range(num_days - 1):
                transition = [
                    work[e, previous_shift, d].Not(),
                    work[e, next_shift, d + 1].Not()
                ]
                if cost == 0:
                    model.AddBoolOr(transition)
                else:
                    trans_var = model.NewBoolVar('')
                    transition.append(trans_var)
                    model.AddBoolOr(transition)
                    obj_vars.append(trans_var)
                    obj_coeffs.append(cost)

    # Cover constraints
    for s in range(1, num_shifts):
        for w in range(num_weeks):
            for d in range(7):
                works = [work[e, s, w * 7 + d] for e in range(num_employees)]
                min_demand = weekly_cover_demands[d][s - 1]
                worked = model.NewIntVar(min_demand, num_employees, '')
                model.Add(worked == sum(works))
                over_penalty = excess_cover_penalties[s - 1]
                if over_penalty > 0:
                    excess = model.NewIntVar(0, num_employees - min_demand, '')
                    model.Add(excess == worked - min_demand)
                    obj_vars.append(excess)
                    obj_coeffs.append(over_penalty)

    # Objective
    model.Minimize(sum(obj_vars[i] * obj_coeffs[i] for i in range(len(obj_vars))))

    # Solve the model.
    solver = cp_model.CpSolver()
    solver.parameters.num_search_workers = 8
    solution_printer = ObjectiveSolutionPrinter()
    status = solver.SolveWithSolutionCallback(model, solution_printer)

    # Print solution.
    if status == cp_model.OPTIMAL or status == cp_model.FEASIBLE:
        print()
        header = '          '
        for w in range(num_weeks):
            header += 'M T W T F S S '
        print(header)
        for e in range(num_employees):
            schedule = ''
            for d in range(num_days):
                for s in range(num_shifts):
                    if solver.BooleanValue(work[e, s, d]):
                        schedule += shifts[s] + ' '
            print('worker %i: %s' % (e, schedule))

    print()
    print('Statistics')
    print('  - status          : %s' % solver.StatusName(status))
    print('  - conflicts       : %i' % solver.NumConflicts())
    print('  - branches        : %i' % solver.NumBranches())
    print('  - wall time       : %f ms' % solver.WallTime())
    print('  - solutions found : %i' % solution_printer.SolutionCount())


if __name__ == '__main__':
    main()
