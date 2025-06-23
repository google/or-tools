#!/usr/bin/env python3
"""Solve a School Scheduling Problem"""
from ortools.sat.python import cp_model


class SchoolSchedulingProblem:
  """Data of the problem."""

  def __init__(
      self,
      levels,
      sections,
      subjects,
      curriculum,
      teachers,
      specialties,
      time_slots,
  ):
    self._levels = levels
    self._sections = sections
    self._subjects = subjects
    self._curriculum = curriculum
    assert len(self._curriculum) == len(self._levels) * len(
        self._subjects
    ), 'Some curriculum are missing'
    for lvl, sub in self._curriculum.keys():
      assert lvl in self._levels, f'{lvl} not in LEVELS'
      assert sub in self._subjects, f'{sub} not in SUBJECTS'

    self._teachers = teachers
    self._specialties = specialties
    assert len(self._specialties) == len(self._subjects), 'Missing some rows'
    for s, ts in self._specialties.items():
      assert s in self._subjects, f'{s} is not in SUBJECTS'
      for t in ts:
        assert t in self._teachers, f'{t} is not in TEACHERS'

    self._time_slots = time_slots

  @property
  def levels(self):
    return self._levels

  @property
  def sections(self):
    return self._sections

  @property
  def subjects(self):
    return self._subjects

  @property
  def curriculum(self):
    return self._curriculum

  @property
  def teachers(self):
    return self._teachers

  def teacher_name(self, teacher_idx):
    assert 0 <= teacher_idx < len(self._teachers)
    return list(self._teachers.keys())[teacher_idx]

  def teacher_max_hours(self, teacher_idx):
    assert 0 <= teacher_idx < len(self._teachers)
    return list(self._teachers.values())[teacher_idx]

  @property
  def specialties(self):
    return self._specialties

  def specialtie_teachers(self, subject):
    assert subject in self._subjects, f'{subject} not in SUBJECTS'
    return self._specialties[subject]

  @property
  def time_slots(self):
    return self._time_slots

  def slot_duration(self, slot_idx):
    assert 0 <= slot_idx < len(self._time_slots)
    return list(self._time_slots.values())[slot_idx]


class SchoolSchedulingSatSolver:
  """Solver instance."""

  def __init__(self, problem: SchoolSchedulingProblem):
    # Problem
    self._problem = problem

    # Utilities
    num_levels = len(self._problem.levels)
    self._all_levels = range(num_levels)
    num_sections = len(self._problem.sections)
    self._all_sections = range(num_sections)
    num_subjects = len(self._problem.subjects)
    self._all_subjects = range(num_subjects)
    num_teachers = len(self._problem.teachers)
    self._all_teachers = range(num_teachers)
    num_slots = len(self._problem.time_slots)
    self._all_slots = range(num_slots)

    # Create Model
    self._model = cp_model.CpModel()

    # Create Variables
    self._assignment = {}
    for lvl_idx, level in enumerate(self._problem.levels):
      for sec_idx, section in enumerate(self._problem.sections):
        for sub_idx, subject in enumerate(self._problem.subjects):
          for tch_idx, teacher in enumerate(self._problem.teachers):
            for slt_idx, slot in enumerate(self._problem.time_slots):
              key = (lvl_idx, sec_idx, sub_idx, tch_idx, slt_idx)
              name = f'{level}-{section} S:{subject} T:{teacher} Slot:{slot}'
              # print(name)
              if teacher in self._problem.specialtie_teachers(subject):
                self._assignment[key] = self._model.NewBoolVar(name)
              else:
                name = 'NO DISP ' + name
                self._assignment[key] = self._model.NewIntVar(0, 0, name)

    # Constraints
    # Each Level-Section must have the quantity of classes per Subject specified in the Curriculum
    for lvl_idx, level in enumerate(self._problem.levels):
      for sec_idx in self._all_sections:
        for sub_idx, subject in enumerate(self._problem.subjects):
          required_duration = self._problem.curriculum[level, subject]
          # print(f'L:{level} S:{subject} duration:{required_duration}h')
          self._model.Add(
              sum(
                  self._assignment[lvl_idx, sec_idx, sub_idx, tch_idx, slt_idx]
                  * int(self._problem.slot_duration(slt_idx) * 10)
                  for tch_idx in self._all_teachers
                  for slt_idx in self._all_slots
              )
              == int(required_duration * 10)
          )

    # Each Level-Section can do at most one class at a time
    for lvl_idx in self._all_levels:
      for sec_idx in self._all_sections:
        for slt_idx in self._all_slots:
          self._model.Add(
              sum([
                  self._assignment[lvl_idx, sec_idx, sub_idx, tch_idx, slt_idx]
                  for sub_idx in self._all_subjects
                  for tch_idx in self._all_teachers
              ])
              <= 1
          )

    # Teacher can do at most one class at a time
    for tch_idx in self._all_teachers:
      for slt_idx in self._all_slots:
        self._model.Add(
            sum([
                self._assignment[lvl_idx, sec_idx, sub_idx, tch_idx, slt_idx]
                for lvl_idx in self._all_levels
                for sec_idx in self._all_sections
                for sub_idx in self._all_subjects
            ])
            <= 1
        )

    # Maximum work hours for each teacher
    for tch_idx in self._all_teachers:
      self._model.Add(
          sum([
              self._assignment[lvl_idx, sec_idx, sub_idx, tch_idx, slt_idx]
              * int(self._problem.slot_duration(slt_idx) * 10)
              for lvl_idx in self._all_levels
              for sec_idx in self._all_sections
              for sub_idx in self._all_subjects
              for slt_idx in self._all_slots
          ])
          <= int(self._problem.teacher_max_hours(tch_idx) * 10)
      )

    # Teacher makes all the classes of a subject's course
    teacher_courses = {}
    for lvl_idx, level in enumerate(self._problem.levels):
      for sec_idx, section in enumerate(self._problem.sections):
        for sub_idx, subject in enumerate(self._problem.subjects):
          for tch_idx, teacher in enumerate(self._problem.teachers):
            name = f'{level}-{section} S:{subject} T:{teacher}'
            teacher_courses[lvl_idx, sec_idx, sub_idx, tch_idx] = (
                self._model.NewBoolVar(name)
            )
            temp_array = [
                self._assignment[lvl_idx, sec_idx, sub_idx, tch_idx, slt_idx]
                for slt_idx in self._all_slots
            ]
            self._model.AddMaxEquality(
                teacher_courses[lvl_idx, sec_idx, sub_idx, tch_idx], temp_array
            )
          self._model.Add(
              sum([
                  teacher_courses[lvl_idx, sec_idx, sub_idx, tch_idx]
                  for tch_idx in self._all_teachers
              ])
              == 1
          )

  def print_teacher_schedule(self, tch_idx):
    teacher_name = self._problem.teacher_name(tch_idx)
    print(f'Teacher: {teacher_name}')
    total_working_hours = 0
    for slt_idx, slot in enumerate(self._problem.time_slots):
      for lvl_idx, level in enumerate(self._problem.levels):
        for sec_idx, section in enumerate(self._problem.sections):
          for sub_idx, subject in enumerate(self._problem.subjects):
            key = (lvl_idx, sec_idx, sub_idx, tch_idx, slt_idx)
            if self._solver.BooleanValue(self._assignment[key]):
              total_working_hours += self._problem.slot_duration(slt_idx)
              print(f'{slot}: C:{level}-{section} S:{subject}')
    print(f'Total working hours: {total_working_hours}h')

  def print_class_schedule(self, lvl_idx, sec_idx):
    level = self._problem.levels[lvl_idx]
    section = self._problem.sections[sec_idx]
    print(f'Class: {level}-{section}')
    total_working_hours = {}
    for sub in self._problem.subjects:
      total_working_hours[sub] = 0
    for slt_idx, slot in enumerate(self._problem.time_slots):
      for tch_idx, teacher in enumerate(self._problem.teachers):
        for sub_idx, subject in enumerate(self._problem.subjects):
          key = (lvl_idx, sec_idx, sub_idx, tch_idx, slt_idx)
          if self._solver.BooleanValue(self._assignment[key]):
            total_working_hours[subject] += self._problem.slot_duration(slt_idx)
            print(f'{slot}: S:{subject} T:{teacher}')
    for subject, hours in total_working_hours.items():
      print(f'Total working hours for {subject}: {hours}h')

  def print_school_schedule(self):
    print('School:')
    for slt_idx, slot in enumerate(self._problem.time_slots):
      tmp = f'{slot}:'
      for lvl_idx, level in enumerate(self._problem.levels):
        for sec_idx, section in enumerate(self._problem.sections):
          for sub_idx, subject in enumerate(self._problem.subjects):
            for tch_idx, teacher in enumerate(self._problem.teachers):
              key = (lvl_idx, sec_idx, sub_idx, tch_idx, slt_idx)
              if self._solver.BooleanValue(self._assignment[key]):
                tmp += f' {level}-{section}:({subject},{teacher})'
      print(tmp)

  def solve(self):
    print('Solving')
    # Create Solver
    self._solver = cp_model.CpSolver()

    solution_printer = SchoolSchedulingSatSolutionPrinter()
    status = self._solver.Solve(self._model, solution_printer)
    print('Status: ', self._solver.StatusName(status))

    if status == cp_model.OPTIMAL or status == cp_model.FEASIBLE:
      print('\n# Teachers')
      for teacher_idx in self._all_teachers:
        self.print_teacher_schedule(teacher_idx)

      print('\n# Classes')
      for level_idx in self._all_levels:
        for section_idx in self._all_sections:
          self.print_class_schedule(level_idx, section_idx)

      print('\n# School')
      self.print_school_schedule()

    print('Branches: ', self._solver.NumBranches())
    print('Conflicts: ', self._solver.NumConflicts())
    print('WallTime: ', self._solver.WallTime())


class SchoolSchedulingSatSolutionPrinter(cp_model.CpSolverSolutionCallback):

  def __init__(self):
    cp_model.CpSolverSolutionCallback.__init__(self)
    self.__solution_count = 0

  def OnSolutionCallback(self):
    print(
        f'Solution #{self.__solution_count}, objective: {self.ObjectiveValue()}'
    )
    self.__solution_count += 1


def main():
  # DATA
  ## Classes
  LEVELS = [
      '1',
      '2',
      '3',
  ]
  SECTIONS = [
      'A',
      'B',
  ]
  SUBJECTS = [
      'English',
      'Math',
      #'Science',
      'History',
  ]
  CURRICULUM = {
      ('1', 'English'): 3,
      ('1', 'Math'): 3,
      ('1', 'History'): 2,
      ('2', 'English'): 4,
      ('2', 'Math'): 2,
      ('2', 'History'): 2,
      ('3', 'English'): 2,
      ('3', 'Math'): 4,
      ('3', 'History'): 2,
  }

  ## Teachers
  TEACHERS = {  # name, max_work_hours
      'Mario': 14,
      'Elvis': 12,
      'Harry': 12,
      'Ian': 14,
  }
  # Subject -> List of teachers who can teach it
  SPECIALTIES = {
      'English': ['Elvis', 'Ian'],
      'Math': ['Mario', 'Ian'],
      'History': ['Harry', 'Ian'],
  }

  ## Schedule
  TIME_SLOTS = {
      'Monday:08:00-09:30': 1.5,
      'Monday:09:45-11:15': 1.5,
      'Monday:11:30-12:30': 1,
      'Monday:13:30-15:30': 2,
      'Monday:15:45-17:15': 1.5,
      'Tuesday:08:00-09:30': 1.5,
      'Tuesday:09:45-11:15': 1.5,
      'Tuesday:11:30-12:30': 1,
      'Tuesday:13:30-15:30': 2,
      'Tuesday:15:45-17:15': 1.5,
      'Wednesday:08:00-09:30': 1.5,
      'Wednesday:09:45-11:15': 1.5,
      'Wednesday:11:30-12:30': 1,
      'Thursday:08:00-09:30': 1.5,
      'Thursday:09:45-11:15': 1.5,
      'Thursday:11:30-12:30': 1,
      'Thursday:13:30-15:30': 2,
      'Thursday:15:45-17:15': 1.5,
      'Friday:08:00-09:30': 1.5,
      'Friday:09:45-11:15': 1.5,
      'Friday:11:30-12:30': 1,
      'Friday:13:30-15:30': 2,
      'Friday:15:45-17:15': 1.5,
  }

  problem = SchoolSchedulingProblem(
      LEVELS, SECTIONS, SUBJECTS, CURRICULUM, TEACHERS, SPECIALTIES, TIME_SLOTS
  )
  solver = SchoolSchedulingSatSolver(problem)
  solver.solve()


if __name__ == '__main__':
  main()
