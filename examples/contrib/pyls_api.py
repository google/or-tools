#!/usr/bin/env python3
from ortools.constraint_solver.python import constraint_solver as cp


class OneVarLns(cp.BaseLns):
    """One Var LNS."""

    def __init__(self, vars):
        cp.BaseLns.__init__(self, vars)
        self.__index = 0

    def init_fragments(self):
        self.__index = 0

    def next_fragment(self):
        if self.__index < self.size():
            self.append_to_fragment(self.__index)
            self.__index += 1
            return True
        else:
            return False


class MoveOneVar(cp.IntVarLocalSearchOperator):
    """Move one var up or down."""

    def __init__(self, vars):
        cp.IntVarLocalSearchOperator.__init__(self, vars)
        self.__index = 0
        self.__up = False

    def one_neighbor(self):
        current_value = self.old_value(self.__index)
        if self.__up:
            self.set_value(self.__index, current_value + 1)
            self.__index = (self.__index + 1) % self.size()
        else:
            self.set_value(self.__index, current_value - 1)
        self.__up = not self.__up
        return True

    def on_start(self):
        pass

    def is_incremental(self):
        return False


class SumFilter(cp.IntVarLocalSearchFilter):
    """Filter to speed up LS computation."""

    def __init__(self, vars):
        cp.IntVarLocalSearchFilter.__init__(self, vars)
        self.__sum = 0

    def on_synchronize(self, delta):
        self.__sum = sum(self.value(index) for index in range(self.size()))

    def accept(
        self,
        delta,
        unused_delta_delta,
        unused_objective_min,
        unused_objective_max,
    ):
        solution_delta = delta.int_var_container()
        solution_delta_size = solution_delta.size()
        for i in range(solution_delta_size):
            if not solution_delta.element(i).activated():
                return True

        new_sum = self.__sum
        for i in range(solution_delta_size):
            element = solution_delta.element(i)
            int_var = element.var()
            touched_var_index = self.index_from_var(int_var)
            old_value = self.value(touched_var_index)
            new_value = element.value()
            new_sum += new_value - old_value

        return new_sum < self.__sum

    def is_incremental(self):
        return False


def Solve(type):
    solver = cp.Solver("Solve")
    vars = [solver.new_int_var(0, 4) for _ in range(4)]
    sum_var = solver.sum(vars).var()
    obj = solver.minimize(sum_var, 1)
    db = solver.phase(
        vars,
        cp.IntVarStrategy.CHOOSE_FIRST_UNBOUND,
        cp.IntValueStrategy.ASSIGN_MAX_VALUE,
    )
    ls = None

    if type == 0:  # LNS
        print("Large Neighborhood Search")
        one_var_lns = OneVarLns(vars)
        ls_params = solver.local_search_phase_parameters(sum_var, one_var_lns, db)
        ls = solver.local_search_phase(vars, db, ls_params)
    elif type == 1:  # LS
        print("Local Search")
        move_one_var = MoveOneVar(vars)
        ls_params = solver.local_search_phase_parameters(sum_var, move_one_var, db)
        ls = solver.local_search_phase(vars, db, ls_params)
    else:
        print("Local Search with Filter")
        move_one_var = MoveOneVar(vars)
        sum_filter = SumFilter(vars)
        filter_manager = cp.LocalSearchFilterManager([sum_filter])
        ls_params = solver.local_search_phase_parameters(
            sum_var, move_one_var, db, None, filter_manager
        )
        ls = solver.local_search_phase(vars, db, ls_params)

    collector = solver.last_solution_collector()
    collector.add(vars)
    collector.add_objective(sum_var)
    log = solver.search_log(1000, obj)
    solver.solve(ls, [collector, obj, log])
    print("Objective value = %d" % collector.objective_value(0))


def main():
    Solve(0)
    Solve(1)
    Solve(2)


if __name__ == "__main__":
    main()
