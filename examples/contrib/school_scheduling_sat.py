from ortools.sat.python import cp_model


class SchoolSchedulingProblem(object):

  def __init__(self, subjects, teachers, curriculum, specialties, working_days,
               periods, levels, sections, teacher_work_hours):
    self.subjects = subjects
    self.teachers = teachers
    self.curriculum = curriculum
    self.specialties = specialties
    self.working_days = working_days
    self.periods = periods
    self.levels = levels
    self.sections = sections
    self.teacher_work_hours = teacher_work_hours


class SchoolSchedulingSatSolver(object):

  def __init__(self, problem):
    # Problem
    self.problem = problem

    # Utilities
    self.timeslots = [
        '{0:10} {1:6}'.format(x, y)
        for x in problem.working_days
        for y in problem.periods
    ]
    self.num_days = len(problem.working_days)
    self.num_periods = len(problem.periods)
    self.num_slots = len(self.timeslots)
    self.num_teachers = len(problem.teachers)
    self.num_subjects = len(problem.subjects)
    self.num_levels = len(problem.levels)
    self.num_sections = len(problem.sections)
    self.courses = [
        x * self.num_levels + y
        for x in problem.levels
        for y in problem.sections
    ]
    self.num_courses = self.num_levels * self.num_sections

    all_courses = range(self.num_courses)
    all_teachers = range(self.num_teachers)
    all_slots = range(self.num_slots)
    all_sections = range(self.num_sections)
    all_subjects = range(self.num_subjects)
    all_levels = range(self.num_levels)

    self.model = cp_model.CpModel()

    self.assignment = {}
    for c in all_courses:
      for s in all_subjects:
        for t in all_teachers:
          for slot in all_slots:
            if t in self.problem.specialties[s]:
              name = 'C:{%i} S:{%i} T:{%i} Slot:{%i}' % (c, s, t, slot)
              self.assignment[c, s, t, slot] = self.model.NewBoolVar(name)
            else:
              name = 'NO DISP C:{%i} S:{%i} T:{%i} Slot:{%i}' % (c, s, t, slot)
              self.assignment[c, s, t, slot] = self.model.NewIntVar(0, 0, name)

    # Constraints

    # Each course must have the quantity of classes specified in the curriculum
    for level in all_levels:
      for section in all_sections:
        course = level * self.num_sections + section
        for subject in all_subjects:
          required_slots = self.problem.curriculum[
              self.problem.levels[level], self.problem.subjects[subject]]
          self.model.Add(
              sum(self.assignment[course, subject, teacher, slot]
                  for slot in all_slots
                  for teacher in all_teachers) == required_slots)

    # Teacher can do at most one class at a time
    for teacher in all_teachers:
      for slot in all_slots:
        self.model.Add(
            sum([
                self.assignment[c, s, teacher, slot]
                for c in all_courses
                for s in all_subjects
            ]) <= 1)

    # Maximum work hours for each teacher
    for teacher in all_teachers:
      self.model.Add(
          sum([
              self.assignment[c, s, teacher, slot] for c in all_courses
              for s in all_subjects for slot in all_slots
          ]) <= self.problem.teacher_work_hours[teacher])

    # Teacher makes all the classes of a subject's course
    teacher_courses = {}
    for level in all_levels:
      for section in all_sections:
        course = level * self.num_sections + section
        for subject in all_subjects:
          for t in all_teachers:
            name = 'C:{%i} S:{%i} T:{%i}' % (course, subject, teacher)
            teacher_courses[course, subject, t] = self.model.NewBoolVar(name)
            temp_array = [
                self.assignment[course, subject, t, slot] for slot in all_slots
            ]
            self.model.AddMaxEquality(teacher_courses[course, subject, t],
                                      temp_array)
          self.model.Add(
              sum(teacher_courses[course, subject, t]
                  for t in all_teachers) == 1)

  def solve(self):
    print('Solving')
    solver = cp_model.CpSolver()
    solution_printer = SchoolSchedulingSatSolutionPrinter()
    status = solver.Solve(self.model, solution_printer)
    print()
    print('status', status)
    print('Branches', solver.NumBranches())
    print('Conflicts', solver.NumConflicts())
    print('WallTime', solver.WallTime())


class SchoolSchedulingSatSolutionPrinter(cp_model.CpSolverSolutionCallback):

  def __init__(self):
    cp_model.CpSolverSolutionCallback.__init__(self)
    self.__solution_count = 0

  def OnSolutionCallback(self):
    print('Found Solution!')


def main():
  # DATA
  subjects = ['English', 'Math', 'History']
  levels = ['1-', '2-', '3-']
  sections = ['A']
  teachers = ['Mario', 'Elvis', 'Donald', 'Ian']
  teachers_work_hours = [18, 12, 12, 18]
  working_days = ['Monday', 'Tuesday', 'Wednesday', 'Thursday', 'Friday']
  periods = ['08:00-09:30', '09:45-11:15', '11:30-13:00']
  curriculum = {
      ('1-', 'English'): 3,
      ('1-', 'Math'): 3,
      ('1-', 'History'): 2,
      ('2-', 'English'): 4,
      ('2-', 'Math'): 2,
      ('2-', 'History'): 2,
      ('3-', 'English'): 2,
      ('3-', 'Math'): 4,
      ('3-', 'History'): 2
  }

  # Subject -> List of teachers who can teach it
  specialties_idx_inverse = [
      [1, 3],  # English   -> Elvis & Ian
      [0, 3],  # Math      -> Mario & Ian
      [2, 3]  # History   -> Donald & Ian
  ]

  problem = SchoolSchedulingProblem(
      subjects, teachers, curriculum, specialties_idx_inverse, working_days,
      periods, levels, sections, teachers_work_hours)
  solver = SchoolSchedulingSatSolver(problem)
  solver.solve()


if __name__ == '__main__':
  main()
