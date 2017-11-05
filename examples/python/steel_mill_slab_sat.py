from __future__ import print_function

import argparse
from ortools.sat.python import cp_model
from ortools.linear_solver import pywraplp
import time

parser = argparse.ArgumentParser()

parser.add_argument('--problem', default = 2, type = int,
                    help = 'Problem id to solve.')
parser.add_argument('--break_symmetries', default = True, type = bool,
                    help = 'Break symmetries between equivalent orders.')
parser.add_argument(
  '--solver', default = "mip_column",
  help =  'Method used to solve: sat, sat_table, sat_column, mip_column.')
parser.add_argument('--output_proto', default = "",
                    help = 'Output file to write the cp_model proto to.')



def BuildProblem(problem_id):
  """Build problem data."""
  if problem_id == 0:
    capacities = [
        0, 12, 14, 17, 18, 19, 20, 23, 24, 25, 26, 27, 28, 29, 30, 32, 35, 39,
        42, 43, 44
    ]
    num_colors = 88
    num_slabs = 111
    orders = [
        (4, 1),  # (size, color)
        (22, 2),
        (9, 3),
        (5, 4),
        (8, 5),
        (3, 6),
        (3, 4),
        (4, 7),
        (7, 4),
        (7, 8),
        (3, 6),
        (2, 6),
        (2, 4),
        (8, 9),
        (5, 10),
        (7, 11),
        (4, 7),
        (7, 11),
        (5, 10),
        (7, 11),
        (8, 9),
        (3, 1),
        (25, 12),
        (14, 13),
        (3, 6),
        (22, 14),
        (19, 15),
        (19, 15),
        (22, 16),
        (22, 17),
        (22, 18),
        (20, 19),
        (22, 20),
        (5, 21),
        (4, 22),
        (10, 23),
        (26, 24),
        (17, 25),
        (20, 26),
        (16, 27),
        (10, 28),
        (19, 29),
        (10, 30),
        (10, 31),
        (23, 32),
        (22, 33),
        (26, 34),
        (27, 35),
        (22, 36),
        (27, 37),
        (22, 38),
        (22, 39),
        (13, 40),
        (14, 41),
        (16, 27),
        (26, 34),
        (26, 42),
        (27, 35),
        (22, 36),
        (20, 43),
        (26, 24),
        (22, 44),
        (13, 45),
        (19, 46),
        (20, 47),
        (16, 48),
        (15, 49),
        (17, 50),
        (10, 28),
        (20, 51),
        (5, 52),
        (26, 24),
        (19, 53),
        (15, 54),
        (10, 55),
        (10, 56),
        (13, 57),
        (13, 58),
        (13, 59),
        (12, 60),
        (12, 61),
        (18, 62),
        (10, 63),
        (18, 64),
        (16, 65),
        (20, 66),
        (12, 67),
        (6, 68),
        (6, 68),
        (15, 69),
        (15, 70),
        (15, 70),
        (21, 71),
        (30, 72),
        (30, 73),
        (30, 74),
        (30, 75),
        (23, 76),
        (15, 77),
        (15, 78),
        (27, 79),
        (27, 80),
        (27, 81),
        (27, 82),
        (27, 83),
        (27, 84),
        (27, 79),
        (27, 85),
        (27, 86),
        (10, 87),
        (3, 88)
    ]
  elif problem_id == 1:
    capacities = [0, 17, 44]
    num_colors = 23
    num_slabs = 30
    orders = [
        (4, 1),  # (size, color)
        (22, 2),
        (9, 3),
        (5, 4),
        (8, 5),
        (3, 6),
        (3, 4),
        (4, 7),
        (7, 4),
        (7, 8),
        (3, 6),
        (2, 6),
        (2, 4),
        (8, 9),
        (5, 10),
        (7, 11),
        (4, 7),
        (7, 11),
        (5, 10),
        (7, 11),
        (8, 9),
        (3, 1),
        (25, 12),
        (14, 13),
        (3, 6),
        (22, 14),
        (19, 15),
        (19, 15),
        (22, 16),
        (22, 17),
        (22, 18),
        (20, 19),
        (22, 20),
        (5, 21),
        (4, 22),
        (10, 23)
    ]
  elif problem_id == 2:
    capacities = [0, 17, 44]
    num_colors = 15
    num_slabs = 20
    orders = [
        (4, 1),  # (size, color)
        (22, 2),
        (9, 3),
        (5, 4),
        (8, 5),
        (3, 6),
        (3, 4),
        (4, 7),
        (7, 4),
        (7, 8),
        (3, 6),
        (2, 6),
        (2, 4),
        (8, 9),
        (5, 10),
        (7, 11),
        (4, 7),
        (7, 11),
        (5, 10),
        (7, 11),
        (8, 9),
        (3, 1),
        (25, 12),
        (14, 13),
        (3, 6),
        (22, 14),
        (19, 15),
        (19, 15)
    ]

  elif problem_id == 3:
    capacities = [0, 17, 44]
    num_colors = 8
    num_slabs = 10
    orders = [
        (4, 1),  # (size, color)
        (22, 2),
        (9, 3),
        (5, 4),
        (8, 5),
        (3, 6),
        (3, 4),
        (4, 7),
        (7, 4),
        (7, 8),
        (3, 6)
    ]

  return (num_slabs, capacities, num_colors, orders)


class SteelMillSlabSolutionPrinter(cp_model.CpSolverSolutionCallback):
  """Print intermediate solutions."""

  def __init__(self, orders, assign, load, loss):
    self.__orders = orders
    self.__assign = assign
    self.__load = load
    self.__loss = loss
    self.__solution_count = 0
    self.__all_orders = range(len(orders))
    self.__all_slabs = range(len(assign[0]))
    self.__start_time = time.time()

  def NewSolution(self):
    current_time = time.time()
    objective = sum(self.Value(l) for l in self.__loss)
    print('Solution %i, time = %f s, objective = %i' %
          (self.__solution_count, current_time - self.__start_time, objective))
    self.__solution_count += 1
    orders_in_slab = [[o for o in self.__all_orders
                       if self.Value(self.__assign[o][s])]
                      for s in self.__all_slabs]
    for s in self.__all_slabs:
      if orders_in_slab[s]:
        line = '  - slab %i, load = %i, loss = %i, orders = [' % (
            s, self.Value(self.__load[s]), self.Value(self.__loss[s]))
        for o in orders_in_slab[s]:
          line += '#%i(w%i, c%i) ' % (
              o, self.__orders[o][0], self.__orders[o][1])
        line += ']'
        print(line)


class SolutionPrinterWithObjective(cp_model.CpSolverSolutionCallback):
  """Print intermediate solutions."""

  def __init__(self):
    self.__solution_count = 0
    self.__start_time = time.time()

  def NewSolution(self):
    current_time = time.time()
    print('Solution %i, time = %f s, objective = %i' %
          (self.__solution_count, current_time - self.__start_time,
           self.ObjectiveValue()))
    self.__solution_count += 1


def SteelMillSlab(problem, break_symmetries, output_proto):
  """Solves the Steel Mill Slab Problem."""
  ### Load problem.
  (num_slabs, capacities, num_colors, orders) = BuildProblem(problem)

  num_orders = len(orders)
  num_capacities = len(capacities)
  all_slabs = range(num_slabs)
  all_colors = range(num_colors)
  all_orders = range(len(orders))
  print('Solving steel mill with %i orders, %i slabs, and %i capacities' %
        (num_orders, num_slabs, num_capacities - 1))

  # Compute auxilliary data.
  widths = [x[0] for x in orders]
  colors = [x[1] for x in orders]
  max_capacity = max(capacities)
  loss_array = [
      min(x for x in capacities if x >= c) - c for c in range(max_capacity + 1)
  ]
  max_loss = max(loss_array)
  orders_per_color = [[o for o in all_orders if colors[o] == c + 1]
                      for c in all_colors]
  unique_color_orders = [
      o for o in all_orders if len(orders_per_color[colors[o] - 1]) == 1
  ]

  ### Model problem.

  # Create the model and the decision variables.
  model = cp_model.CpModel()
  assign = [[
      model.NewBoolVar('assign_%i_to_slab_%i' % (o, s)) for s in all_slabs
  ] for o in all_orders]
  loads = [
      model.NewIntVar(0, max_capacity, 'load_of_slab_%i' % s) for s in all_slabs
  ]
  color_is_in_slab = [[
      model.NewBoolVar('color_%i_in_slab_%i' % (c + 1, s)) for c in all_colors
  ] for s in all_slabs]

  # Compute load of all slabs.
  for s in all_slabs:
    model.Add(sum(assign[o][s] * widths[o] for o in all_orders) == loads[s])

  # Orders are assigned to one slab.
  for o in all_orders:
    model.Add(sum(assign[o]) == 1)

  # Redundant constraint (sum of loads == sum of widths).
  model.Add(sum(loads) == sum(widths))

  # Link present_colors and assign.
  for c in all_colors:
    for s in all_slabs:
      for o in orders_per_color[c]:
        model.AddImplication(assign[o][s], color_is_in_slab[s][c])
        model.AddImplication(color_is_in_slab[s][c].Not(), assign[o][s].Not())

  # At most two colors per slab.
  for s in all_slabs:
    model.Add(sum(color_is_in_slab[s]) <= 2)

  # Project previous constraint on unique_color_orders
  for s in all_slabs:
    model.Add(sum(assign[o][s] for o in unique_color_orders) <= 2)

  # Symmetry breaking.
  for s in range(num_slabs - 1):
    model.Add(loads[s] >= loads[s + 1])

  # Collect equivalent orders.
  width_to_unique_color_order = {}
  ordered_equivalent_orders = []
  for c in all_colors:
    colored_orders = orders_per_color[c]
    if not colored_orders:
      continue
    if len(colored_orders) == 1:
      o = colored_orders[0]
      w = widths[o]
      if w not in width_to_unique_color_order:
        width_to_unique_color_order[w] = [o]
      else:
        width_to_unique_color_order[w].append(o)
    else:
      local_width_to_order = {}
      for o in colored_orders:
        w = widths[o]
        if w not in local_width_to_order:
          local_width_to_order[w] = []
        local_width_to_order[w].append(o)
      for w, os in local_width_to_order.items():
        if len(os) > 1:
          for p in range(len(os) - 1):
            ordered_equivalent_orders.append((os[p], os[p + 1]))
  for w, os in width_to_unique_color_order.items():
    if len(os) > 1:
      for p in range(len(os) - 1):
        ordered_equivalent_orders.append((os[p], os[p + 1]))

  # Create position variables if there are symmetries to be broken.
  if break_symmetries and ordered_equivalent_orders:
    print('  - creating %i symmetry breaking constraints' %
          len(ordered_equivalent_orders))
    positions = {}
    for p in ordered_equivalent_orders:
      if p[0] not in positions:
        positions[p[0]] = model.NewIntVar(0, num_slabs - 1,
                                          'position_of_slab_%i' % p[0])
        model.AddMapDomain(positions[p[0]], assign[p[0]])
      if p[1] not in positions:
        positions[p[1]] = model.NewIntVar(0, num_slabs - 1,
                                          'position_of_slab_%i' % p[1])
        model.AddMapDomain(positions[p[1]], assign[p[1]])
      # Finally add the symmetry breaking constraint.
      model.Add(positions[p[0]] <= positions[p[1]])

  # Objective.
  obj = model.NewIntVar(0, num_slabs * max_loss, 'obj')
  losses = [model.NewIntVar(0, max_loss, 'loss_%i' % s) for s in all_slabs]
  for s in all_slabs:
    model.AddElement(loads[s], loss_array, losses[s])
  model.Add(obj == sum(losses))
  model.Minimize(obj)

  ### Solve model.
  solver = cp_model.CpSolver()
  status = solver.Solve(model)

  ### Output the solution.
  if status == cp_model.OPTIMAL:
    print('Loss = %i, time = %f s, %i conflicts' %
          (solver.ObjectiveValue(), solver.WallTime(), solver.NumConflicts()))
  else:
    print('No solution')


class AllSolutionsCollector(cp_model.CpSolverSolutionCallback):
  """Collect all solutions callback."""

  def __init__(self, variables):
    self.__solutions = []
    self.__variables = variables

  def NewSolution(self):
    solution = [self.Value(v) for v in self.__variables]
    self.__solutions.append(tuple(solution))

  def AllSolutions(self):
    return self.__solutions


def CollectValidSlabs(capacities, colors, widths, loss_array, all_colors):
  """Collect valid columns (assign, loss) for one slab."""
  all_orders = range(len(colors))
  max_capacity = max(capacities)
  orders_per_color = [[o for o in all_orders if colors[o] == c + 1]
                      for c in all_colors]

  model = cp_model.CpModel()
  assign = [model.NewBoolVar('assign_%i' % o) for o in all_orders]
  load = model.NewIntVar(0, max_capacity, 'load')

  color_in_slab = [model.NewBoolVar('color_%i' % (c + 1)) for c in all_colors]

  # Compute load.
  model.Add(sum(assign[o] * widths[o] for o in all_orders) == load)

  # Link present_colors and assign.
  for c in all_colors:
    for o in orders_per_color[c]:
      model.AddImplication(assign[o], color_in_slab[c])
      model.AddImplication(color_in_slab[c].Not(), assign[o].Not())
    model.AddBoolOr([color_in_slab[c].Not()] +
                    [assign[o] for o in orders_per_color[c]])

  # At most two colors per slab.
  model.Add(sum(color_in_slab) <= 2)

  # Compute loss.
  loss = model.NewIntVar(0, max(loss_array), 'loss')
  model.AddElement(load, loss_array, loss)

  print('Model created')

  # Output model proto to file.
  if output_proto:
    f = open(output_proto, 'w')
    f.write(str(model.ModelProto()))
    f.close()

  ### Solve model and collect columns.
  solver = cp_model.CpSolver()
  collector = AllSolutionsCollector(assign + [loss, load])
  solver.SearchForAllSolutions(model, collector)
  return collector.AllSolutions()


def SteelMillSlabWithValidSlabs(problem, break_symmetries, output_proto):
  """Solves the Steel Mill Slab Problem."""
  ### Load problem.
  (num_slabs, capacities, num_colors, orders) = BuildProblem(problem)

  num_orders = len(orders)
  num_capacities = len(capacities)
  all_slabs = range(num_slabs)
  all_colors = range(num_colors)
  all_orders = range(len(orders))
  print('Solving steel mill with %i orders, %i slabs, and %i capacities' %
        (num_orders, num_slabs, num_capacities - 1))

  # Compute auxilliary data.
  widths = [x[0] for x in orders]
  colors = [x[1] for x in orders]
  max_capacity = max(capacities)
  loss_array = [
      min(x for x in capacities if x >= c) - c for c in range(max_capacity + 1)
  ]
  max_loss = max(loss_array)

  ### Model problem.

  # Create the model and the decision variables.
  model = cp_model.CpModel()
  assign = [[
      model.NewBoolVar('assign_%i_to_slab_%i' % (o, s)) for s in all_slabs
  ] for o in all_orders]
  loads = [model.NewIntVar(0, max_capacity, 'load_%i' % s) for s in all_slabs]
  losses = [model.NewIntVar(0, max_loss, 'loss_%i' % s) for s in all_slabs]

  unsorted_valid_slabs = CollectValidSlabs(capacities, colors, widths,
                                           loss_array, all_colors)
  # Sort slab by descending load/loss. Remove duplicates.
  valid_slabs = sorted(unsorted_valid_slabs,
                       key = lambda c : 1000 * c[-1] + c[-2])
  num_valid_slabs = len(valid_slabs)
  print('  - %i valid slab combinations' % num_valid_slabs)

  for s in all_slabs:
    model.AddAllowedAssignments(
        [assign[o][s] for o in all_orders] + [losses[s], loads[s]],
        valid_slabs)

  # Orders are assigned to one slab.
  for o in all_orders:
    model.Add(sum(assign[o]) == 1)

  # Redundant constraint (sum of loads == sum of widths).
  model.Add(sum(loads) == sum(widths))

  # Symmetry breaking.
  for s in range(num_slabs - 1):
    model.Add(loads[s] >= loads[s + 1])

  # Collect equivalent orders.
  if break_symmetries:
    print('Breaking symmetries')
    width_to_unique_color_order = {}
    ordered_equivalent_orders = []
    orders_per_color = [[o for o in all_orders if colors[o] == c + 1]
                      for c in all_colors]
    for c in all_colors:
      colored_orders = orders_per_color[c]
      if not colored_orders:
        continue
      if len(colored_orders) == 1:
        o = colored_orders[0]
        w = widths[o]
        if w not in width_to_unique_color_order:
          width_to_unique_color_order[w] = [o]
        else:
          width_to_unique_color_order[w].append(o)
      else:
        local_width_to_order = {}
        for o in colored_orders:
          w = widths[o]
          if w not in local_width_to_order:
            local_width_to_order[w] = []
            local_width_to_order[w].append(o)
        for w, os in local_width_to_order.items():
          if len(os) > 1:
            for p in range(len(os) - 1):
              ordered_equivalent_orders.append((os[p], os[p + 1]))
    for w, os in width_to_unique_color_order.items():
      if len(os) > 1:
        for p in range(len(os) - 1):
          ordered_equivalent_orders.append((os[p], os[p + 1]))

    # Create position variables if there are symmetries to be broken.
    if ordered_equivalent_orders:
      print('  - creating %i symmetry breaking constraints' %
            len(ordered_equivalent_orders))
      positions = {}
      for p in ordered_equivalent_orders:
        if p[0] not in positions:
          positions[p[0]] = model.NewIntVar(0, num_slabs - 1,
                                            'position_of_slab_%i' % p[0])
          model.AddMapDomain(positions[p[0]], assign[p[0]])
        if p[1] not in positions:
          positions[p[1]] = model.NewIntVar(0, num_slabs - 1,
                                            'position_of_slab_%i' % p[1])
          model.AddMapDomain(positions[p[1]], assign[p[1]])
          # Finally add the symmetry breaking constraint.
        model.Add(positions[p[0]] <= positions[p[1]])

  # Objective.
  obj = model.NewIntVar(0, num_slabs * max_loss, 'obj')
  model.Add(obj == sum(losses))
  model.Minimize(obj)

  print('Model created')

  # Output model proto to file.
  if output_proto:
    f = open(output_proto, 'w')
    f.write(str(model.ModelProto()))
    f.close()

  ### Solve model.
  solver = cp_model.CpSolver()
  solution_printer = SteelMillSlabSolutionPrinter(orders, assign, loads, losses)
  status = solver.SolveWithSolutionObserver(model, solution_printer)

  ### Output the solution.
  if status == cp_model.OPTIMAL:
    print('Loss = %i, time = %f s, %i conflicts' %
          (solver.ObjectiveValue(), solver.WallTime(), solver.NumConflicts()))
  else:
    print('No solution')


def SteelMillSlabWithColumnGeneration(problem, output_proto):
  """Solves the Steel Mill Slab Problem."""
  ### Load problem.
  (num_slabs, capacities, num_colors, orders) = BuildProblem(problem)

  num_orders = len(orders)
  num_capacities = len(capacities)
  all_slabs = range(num_slabs)
  all_colors = range(num_colors)
  all_orders = range(len(orders))
  print('Solving steel mill with %i orders, %i slabs, and %i capacities' %
        (num_orders, num_slabs, num_capacities - 1))

  # Compute auxilliary data.
  widths = [x[0] for x in orders]
  colors = [x[1] for x in orders]
  max_capacity = max(capacities)
  loss_array = [
      min(x for x in capacities if x >= c) - c for c in range(max_capacity + 1)
  ]

  ### Model problem.

  # Generate all valid slabs (columns)
  unsorted_valid_slabs = CollectValidSlabs(capacities, colors, widths,
                                           loss_array, all_colors)
  # Sort slab by descending load/loss. Remove duplicates.
  valid_slabs = sorted(unsorted_valid_slabs,
                       key = lambda c : 1000 * c[-1] + c[-2])
  num_valid_slabs = len(valid_slabs)
  all_valid_slabs = range(num_valid_slabs)
  print('  - %i valid slab combinations' % num_valid_slabs)

  # create model and decision variables.
  model = cp_model.CpModel()
  selected = [model.NewBoolVar('selected_%i' % i) for i in all_valid_slabs]

  for o in all_orders:
    model.Add(sum(selected[i] for i in all_valid_slabs
                  if valid_slabs[i][o]) == 1)


  # Redundant constraint (sum of loads == sum of widths).
  model.Add(sum(selected[i] * valid_slabs[i][-1] for i in all_valid_slabs) ==
            sum(widths))

  # Objective.
  max_loss = max(valid_slabs[i][-2] for i in all_valid_slabs)
  obj = model.NewIntVar(0, num_slabs * max_loss, 'obj')
  model.Add(obj == sum(selected[i] * valid_slabs[i][-2]
                       for i in all_valid_slabs))
  model.Minimize(obj)

  print('Model created')

  # Output model proto to file.
  if output_proto:
    f = open(output_proto, 'w')
    f.write(str(model.ModelProto()))
    f.close()

  ### Solve model.
  solver = cp_model.CpSolver()
  solution_printer = SolutionPrinterWithObjective()
  status = solver.SolveWithSolutionObserver(model, solution_printer)

  ### Output the solution.
  if status == cp_model.OPTIMAL:
    print('Loss = %i, time = %f s, %i conflicts' %
          (solver.ObjectiveValue(), solver.WallTime(), solver.NumConflicts()))
  else:
    print('No solution')


def SteelMillSlabWithMipColumnGeneration(problem):
  """Solves the Steel Mill Slab Problem."""
  ### Load problem.
  (num_slabs, capacities, num_colors, orders) = BuildProblem(problem)

  num_orders = len(orders)
  num_capacities = len(capacities)
  all_slabs = range(num_slabs)
  all_colors = range(num_colors)
  all_orders = range(len(orders))
  print('Solving steel mill with %i orders, %i slabs, and %i capacities' %
        (num_orders, num_slabs, num_capacities - 1))

  # Compute auxilliary data.
  widths = [x[0] for x in orders]
  colors = [x[1] for x in orders]
  max_capacity = max(capacities)
  loss_array = [
      min(x for x in capacities if x >= c) - c for c in range(max_capacity + 1)
  ]

  ### Model problem.

  # Generate all valid slabs (columns)
  start = time.time()
  unsorted_valid_slabs = CollectValidSlabs(capacities, colors, widths,
                                           loss_array, all_colors)
  # Sort slab by descending load/loss. Remove duplicates.
  valid_slabs = sorted(unsorted_valid_slabs,
                       key = lambda c : 1000 * c[-1] + c[-2])
  num_valid_slabs = len(valid_slabs)
  all_valid_slabs = range(num_valid_slabs)
  generate = time.time()
  print('  - %i valid slab combinations generated in %f s' % (
    num_valid_slabs, generate - start))

  # create model and decision variables.
  solver = pywraplp.Solver('Steel',
                           pywraplp.Solver.BOP_INTEGER_PROGRAMMING)
  selected = [solver.IntVar(0.0, 1.0, 'selected_%i' % i)
                            for i in all_valid_slabs]

  for o in all_orders:
    solver.Add(sum(selected[i] for i in all_valid_slabs
                   if valid_slabs[i][o]) == 1)


  # Redundant constraint (sum of loads == sum of widths).
  solver.Add(sum(selected[i] * valid_slabs[i][-1] for i in all_valid_slabs) ==
             sum(widths))

  # Objective.
  solver.Minimize(sum(selected[i] * valid_slabs[i][-2]
                      for i in all_valid_slabs))

  status = solver.Solve()

  ### Output the solution.
  if status == pywraplp.Solver.OPTIMAL:
    print('Objective value = %f found in %f s' % (
      solver.Objective().Value(), time.time() - generate))
  else:
    print('No solution')


def main(args):
  if args.solver == 'sat':
    SteelMillSlab(args.problem, args.break_symmetries, args.output_proto)
  elif args.solver == 'sat_table':
    SteelMillSlabWithValidSlabs(args.problem, args.break_symmetries,
                                args.output_proto)
  elif args.solver == 'sat_column':
    SteelMillSlabWithColumnGeneration(args.problem, args.output_proto)
  else:  # 'mip_column'
    SteelMillSlabWithMipColumnGeneration(args.problem)


if __name__ == '__main__':
  main(parser.parse_args())
