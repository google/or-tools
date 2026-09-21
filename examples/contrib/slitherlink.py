from collections import deque

from ortools.constraint_solver.python import constraint_solver as cp

small = [[3, 2, -1, 3], [-1, -1, -1, 2], [3, -1, -1, -1], [3, -1, 3, 1]]

medium = [
    [-1, 0, -1, 1, -1, -1, 1, -1],
    [-1, 3, -1, -1, 2, 3, -1, 2],
    [-1, -1, 0, -1, -1, -1, -1, 0],
    [-1, 3, -1, -1, 0, -1, -1, -1],
    [-1, -1, -1, 3, -1, -1, 0, -1],
    [1, -1, -1, -1, -1, 3, -1, -1],
    [3, -1, 1, 3, -1, -1, 3, -1],
    [-1, 0, -1, -1, 3, -1, 3, -1],
]

big = [
    [3, -1, -1, -1, 2, -1, 1, -1, 1, 2],
    [1, -1, 0, -1, 3, -1, 2, 0, -1, -1],
    [-1, 3, -1, -1, -1, -1, -1, -1, 3, -1],
    [2, 0, -1, 3, -1, 2, 3, -1, -1, -1],
    [-1, -1, -1, 1, 1, 1, -1, -1, 3, 3],
    [2, 3, -1, -1, 2, 2, 3, -1, -1, -1],
    [-1, -1, -1, 1, 2, -1, 2, -1, 3, 3],
    [-1, 2, -1, -1, -1, -1, -1, -1, 2, -1],
    [-1, -1, 1, 1, -1, 2, -1, 1, -1, 3],
    [3, 3, -1, 1, -1, 2, -1, -1, -1, 2],
]


def NeighboringArcs(i, j, h_arcs, v_arcs):
    tmp = []
    if j > 0:
        tmp.append(h_arcs[i][j - 1])
    if j < len(v_arcs) - 1:
        tmp.append(h_arcs[i][j])
    if i > 0:
        tmp.append(v_arcs[j][i - 1])
    if i < len(h_arcs) - 1:
        tmp.append(v_arcs[j][i])
    return tmp


def PrintSolution(data, h_arcs, v_arcs):
    num_rows = len(data)
    num_columns = len(data[0])

    for i in range(num_rows):
        first_line = ""
        second_line = ""
        third_line = ""
        for j in range(num_columns):
            h_arc = h_arcs[i][j].value()
            v_arc = v_arcs[j][i].value()
            cnt = data[i][j]
            first_line += " ---" if h_arc else "    "
            second_line += "|" if v_arc else " "
            second_line += "   " if cnt == -1 else " %i " % cnt
            third_line += "|   " if v_arc == 1 else "    "
        termination = v_arcs[num_columns][i].value()
        second_line += "|" if termination else " "
        third_line += "|" if termination else " "
        print(first_line)
        print(third_line)
        print(second_line)
        print(third_line)
    last_line = ""
    for j in range(num_columns):
        h_arc = h_arcs[num_rows][j].value()
        last_line += " ---" if h_arc else "    "
    print(last_line)


class BooleanSumEven(cp.PyConstraint):

    def __init__(self, solver, vars):
        cp.PyConstraint.__init__(self, solver)
        self.__vars = vars
        self.__num_possible_true_vars = cp.RevInteger(0)
        self.__num_always_true_vars = cp.RevInteger(0)

    def post(self):
        for i in range(len(self.__vars)):
            v = self.__vars[i]
            if not v.bound():
                demon = self.demon(BooleanSumEven.Update, i)
                v.when_bound(demon)

    def initial_propagate(self):
        num_always_true = 0
        num_possible_true = 0
        possible_true_index = -1
        for i in range(len(self.__vars)):
            var = self.__vars[i]
            if var.min() == 1:
                num_always_true += 1
                num_possible_true += 1
            elif var.max() == 1:
                num_possible_true += 1
                possible_true_index = i

        if num_always_true == num_possible_true and num_possible_true % 2 == 1:
            self.solver.fail()

        if num_possible_true == num_always_true + 1:
            self.__vars[possible_true_index].set_value(num_always_true % 2)

        self.__num_possible_true_vars.set_value(self.solver, num_possible_true)
        self.__num_always_true_vars.set_value(self.solver, num_always_true)

    def Update(self, index):
        solver = self.solver
        value = self.__vars[index].value()
        if value == 0:
            self.__num_possible_true_vars.decr(solver)
        else:
            self.__num_always_true_vars.incr(solver)

        num_possible = self.__num_possible_true_vars.value()
        num_always = self.__num_always_true_vars.value()

        if num_always == num_possible and num_possible % 2 == 1:
            solver.fail()

        if num_possible == num_always + 1:
            possible_true_index = -1
            for i in range(len(self.__vars)):
                if not self.__vars[i].bound():
                    possible_true_index = i
                    break

            if possible_true_index != -1:
                self.__vars[possible_true_index].set_value(num_always % 2)

    def __str__(self):
        return "BooleanSumEven"


# Dedicated constraint: There is a single path on the grid.
# This constraint does not enforce the non-crossing, this is done
# by the constraint on the degree of each node.
class GridSinglePath(cp.PyConstraint):

    def __init__(self, solver, h_arcs, v_arcs):
        cp.PyConstraint.__init__(self, solver)
        self.__h_arcs = h_arcs
        self.__v_arcs = v_arcs

    def post(self):
        demon = self.delayed_initial_propagate_demon()
        for row in self.__h_arcs:
            for var in row:
                var.when_bound(demon)

        for column in self.__v_arcs:
            for var in column:
                var.when_bound(demon)

    # This constraint implements a single propagation.
    # If one point is on the path, it checks the reachability of all possible
    # nodes, and zero out the unreachable parts.
    def initial_propagate(self):
        num_rows = len(self.__h_arcs)
        num_columns = len(self.__v_arcs)

        num_points = num_rows * num_columns
        root_node = -1
        possible_points = set()
        neighbors = [[] for _ in range(num_points)]

        for i in range(num_rows):
            for j in range(num_columns - 1):
                h_arc = self.__h_arcs[i][j]
                if h_arc.max() == 1:
                    head = i * num_columns + j
                    tail = i * num_columns + j + 1
                    neighbors[head].append(tail)
                    neighbors[tail].append(head)
                    possible_points.add(head)
                    possible_points.add(tail)
                    if root_node == -1 and h_arc.min() == 1:
                        root_node = head

        for i in range(num_rows - 1):
            for j in range(num_columns):
                v_arc = self.__v_arcs[j][i]
                if v_arc.max() == 1:
                    head = i * num_columns + j
                    tail = (i + 1) * num_columns + j
                    neighbors[head].append(tail)
                    neighbors[tail].append(head)
                    possible_points.add(head)
                    possible_points.add(tail)
                    if root_node == -1 and v_arc.min() == 1:
                        root_node = head

        if root_node == -1:
            return

        visited_points = set()
        to_process = deque()

        # Compute reachable points
        to_process.append(root_node)
        while to_process:
            candidate = to_process.popleft()
            visited_points.add(candidate)
            for neighbor in neighbors[candidate]:
                if not neighbor in visited_points:
                    to_process.append(neighbor)
                    visited_points.add(neighbor)

        if len(visited_points) < len(possible_points):
            for point in visited_points:
                possible_points.remove(point)

            # Loop on unreachable points and zero all neighboring arcs.
            for point in possible_points:
                i = point // num_columns
                j = point % num_columns
                neighbors = NeighboringArcs(i, j, self.__h_arcs, self.__v_arcs)
                for var in neighbors:
                    var.set_max(0)


def SlitherLink(data):
    num_rows = len(data)
    num_columns = len(data[0])

    solver = cp.Solver("slitherlink")
    h_arcs = [
        [solver.new_bool_var("h_arcs[%i][%i]" % (i, j)) for j in range(num_columns)]
        for i in range(num_rows + 1)
    ]

    v_arcs = [
        [solver.new_bool_var("v_arcs[%i][%i]" % (i, j)) for j in range(num_rows)]
        for i in range(num_columns + 1)
    ]

    # Constraint on the sum or arcs
    for i in range(num_rows):
        for j in range(num_columns):
            if data[i][j] != -1:
                sq = [h_arcs[i][j], h_arcs[i + 1][j], v_arcs[j][i], v_arcs[j + 1][i]]
                solver.add_sum_equality(sq, data[i][j])

    # Single loop: each node has a degree 0 or 2
    zero_or_two = [0, 2]
    for i in range(num_rows + 1):
        for j in range(num_columns + 1):
            neighbors = NeighboringArcs(i, j, h_arcs, v_arcs)
            solver.add(solver.sum(neighbors).member(zero_or_two))

    # Single loop: sum or arcs on row or column is even
    for i in range(num_columns):
        column = [h_arcs[j][i] for j in range(num_rows + 1)]
        solver.add(BooleanSumEven(solver, column))

    for i in range(num_rows):
        row = [v_arcs[j][i] for j in range(num_columns + 1)]
        solver.add(BooleanSumEven(solver, row))

    # Single loop: main constraint
    solver.add(GridSinglePath(solver, h_arcs, v_arcs))

    # Special rule on corners: value == 3 implies 2 border arcs used.
    if data[0][0] == 3:
        h_arcs[0][0].set_min(1)
        v_arcs[0][0].set_min(1)
    if data[0][num_columns - 1] == 3:
        h_arcs[0][num_columns - 1].set_min(1)
        v_arcs[num_columns][0].set_min(1)
    if data[num_rows - 1][0] == 3:
        h_arcs[num_rows][0].set_min(1)
        v_arcs[0][num_rows - 1].set_min(1)
    if data[num_rows - 1][num_columns - 1] == 3:
        h_arcs[num_rows][num_columns - 1].set_min(1)
        v_arcs[num_columns][num_rows - 1].set_min(1)

    # Search
    all_vars = []
    for row in h_arcs:
        all_vars.extend(row)
    for column in v_arcs:
        all_vars.extend(column)

    db = solver.phase(
        all_vars,
        cp.IntVarStrategy.CHOOSE_FIRST_UNBOUND,
        cp.IntValueStrategy.ASSIGN_MAX_VALUE,
    )

    log = solver.search_log(1000000)

    solver.new_search(db, [log])
    while solver.next_solution():
        PrintSolution(data, h_arcs, v_arcs)
    solver.end_search()


if __name__ == "__main__":
    SlitherLink(small)
    SlitherLink(medium)
    SlitherLink(big)
