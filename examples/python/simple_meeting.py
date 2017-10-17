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

"""Simple meeting scheduler.

In this model, we try to schedule a meeting.  The meeting will occur
in the following day. Time is divided in quarters or hours.  It
starts at 8AM and ends at 7PM -> 44 timeslots.

There are p persons involved, m are mandatory. Each has his own set of
meetings. There are r rooms. Each has its own capacity.

Each person must have an 1 hour lunch break between 11h30 AM and 2h30 PM.
The goal is to find a meeting and a room such that all m mandatory people are
in the meeting and a maximum of non mandatory people are also in the meeting.
"""
from __future__ import print_function
from ortools.constraint_solver import pywrapcp


# pylint: disable=too-many-statements
def main():
  # Create the solver.
  solver = pywrapcp.Solver('simple meeting scheduler')

  # All participants (mandatory and non mandatory).
  attendees = 12
  all_people = list(range(1, attendees + 1))

  # Mandatory people.
  all_mandatory_people = [1, 2, 5, 9]

  # Time range.
  max_time = 44

  # Lunch description.
  lunch_start_quarter = 14  # 11:30 AM
  lunch_end_quarter = 25    # 2:30 PM
  lunch_duration = 4        # 1 hour

  # Predefined meetings.
  existing_meetings = [{'start': 2, 'duration': 12, 'person': 1},
                       {'start': 4, 'duration': 8, 'person': 4}]

  rooms_count = 5
  all_rooms = list(range(1, rooms_count + 1))

  room_sizes = {1: 7, 2: 8, 3: 10, 4: 3, 5: 8}

  room_reservations = [{'start': 3, 'duration': 4, 'room': 1},
                       {'start': 20, 'duration': 22, 'room': 3},
                       {'start': 1, 'duration': 18, 'room': 2}]

  for t in existing_meetings:
    m = solver.FixedInterval(t['start'], t['duration'], 'existing meeting')
    t['meeting'] = m

  for t in room_reservations:
    m = solver.FixedInterval(t['start'], t['duration'], 'room reservation')
    t['reservation'] = m

  # Meeting requirements, variables and copies.
  meeting_duration = 3

  meeting = solver.FixedDurationIntervalVar(1,
                                            max_time,
                                            meeting_duration,
                                            False,
                                            'meeting')

  people_meeting_copies = {}
  for p in all_people:
    name = 'people meeting copy %d' % p
    people_meeting_copy = solver.FixedDurationIntervalVar(1,
                                                          max_time,
                                                          meeting_duration,
                                                          True,
                                                          name)
    people_meeting_copies[p] = people_meeting_copy

  room_meeting_copies = {}
  for r in all_rooms:
    name = 'room meeting copy %d' % r
    room_meeting_copy = solver.FixedDurationIntervalVar(1,
                                                        max_time,
                                                        meeting_duration,
                                                        True,
                                                        name)
    room_meeting_copies[r] = room_meeting_copy

  meeting_location = solver.IntVar(list(all_rooms), 'meeting location')

  all_people_calendars = {}
  all_people_presence = {}

  for p in all_people:
    lunch = solver.FixedDurationIntervalVar(lunch_start_quarter,
                                            lunch_end_quarter - lunch_duration,
                                            lunch_duration,
                                            False,
                                            'lunch for %d' % p)
    calendar = [people_meeting_copies[p], lunch]
    calendar.extend([t['meeting']
                     for t in existing_meetings if t['person'] == p])
    disj = solver.DisjunctiveConstraint(calendar, 'people %d calendar' % p)
    all_people_calendars[p] = disj.SequenceVar()
    solver.Add(disj)
    all_people_presence[p] = solver.BoolVar('presence of people %d' % p)

  people_count = solver.IntVar(0, attendees, 'people count')

  all_rooms_calendars = {}
  all_rooms_presence = {}
  for r in all_rooms:
    calendar = [room_meeting_copies[r]]
    calendar.extend([t['reservation']
                     for t in room_reservations if t['room'] == r])
    disj = solver.DisjunctiveConstraint(calendar, 'room %d calendar' % r)
    all_rooms_calendars[r] = disj.SequenceVar()
    solver.Add(disj)
    all_rooms_presence[r] = solver.BoolVar('presence of room %d' % r)

  # Objective: maximum number of people.
  objective = solver.Maximize(people_count, 1)

  # Constraints:

  #  Maintains persons_count.
  solver.Add(people_count == solver.Sum([all_people_presence[p]
                                         for p in all_people]))
  # Mandatory persons.
  for p in all_mandatory_people:
    # pylint: disable=g-explicit-bool-comparison
    solver.Add(all_people_presence[p] == True)

  # All copies are in sync with the original meeting.
  for p in all_people:
    solver.Add(meeting.StaysInSync(people_meeting_copies[p]))

  for r in all_rooms:
    solver.Add(meeting.StaysInSync(room_meeting_copies[r]))

  # Synchronize persons_presence and meetings.
  for p in all_people:
    solver.Add(all_people_presence[p] ==
               people_meeting_copies[p].PerformedExpr())

  # Synchronize all_presences and meetings.
  for r in all_rooms:
    solver.Add(all_rooms_presence[r] ==
               room_meeting_copies[r].PerformedExpr())

  # Map meeting_location to room_meeting presence.
  solver.Add((meeting_location - 1).MapTo([all_rooms_presence[p]
                                           for p in all_rooms]))
  # Persons must fit in the room.
  sizes = [room_sizes[r] for r in all_rooms]
  solver.Add(people_count <= (meeting_location - 1).IndexOf(sizes))

  # Add sequence constraints.
  all_calendars = []
  all_calendars.extend([all_people_calendars[p] for p in all_people])
  all_calendars.extend([all_rooms_calendars[r] for r in all_rooms])

  # Build decision builder.
  # Chosse the location.
  vars_phase = solver.Phase([meeting_location, people_count],
                            solver.INT_VAR_SIMPLE,
                            solver.INT_VALUE_SIMPLE)

  # Solve people calendars conflicts.
  sequence_phase = solver.Phase(all_calendars, solver.SEQUENCE_DEFAULT)
  # Schedule meeting at the earliest possible time.
  meeting_phase = solver.Phase([meeting], solver.INTERVAL_DEFAULT)
  # And compose.
  main_phase = solver.Compose([sequence_phase, vars_phase, meeting_phase])

  solution = solver.Assignment()
  solution.Add(meeting_location)
  solution.Add(people_count)
  solution.Add([all_people_presence[p] for p in all_people])
  solution.Add(meeting)

  collector = solver.LastSolutionCollector(solution)

  search_log = solver.SearchLog(100, people_count)

  solver.Solve(main_phase, [collector, search_log, objective])

  if collector.SolutionCount() > 0:

    print('we could schedule %d persons in room %d starting at quarter %d' %
          (collector.Value(0, people_count),
          collector.Value(0, meeting_location),
          collector.StartValue(0, meeting)))


if __name__ == '__main__':
  main()
