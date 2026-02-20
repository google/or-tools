#!/usr/bin/env python3
# Copyright 2010-2025 Google LLC
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

"""Horse Jumping Show.

A major three-day horse jumping competition is scheduled next winter in Geneva.
The show features riders and horses from all over the world, competing in
several different competitions throughout the show. Six months before the show,
riders submit the entries (i.e., rider name, horse, competition) to the
organizers. Riders can submit multiple entries, for example, to compete in the
same competition with multiple horses, or to compete in several competitions.

There are additional space limitations. For example, the venue has 100 stalls,
4 arenas (where competitions can be scheduled), and 6 paddocks (where riders
warm up before their turn). It is also ideal that paddocks are not overloaded by
riders from multiple competitions.

The organizer's goal is find a schedule in which competitions don't overlap, and
the times at which they happen are scattered throughout the day (and hopefully
not that early in the morning). The starting times of the competitions should be
at the hour or 30 minutes past the hour (e.g. 9:30, 10:00, 10:30, etc.).
Competitions can only be scheduled while there is daylight, except for
competitions scheduled in the Main Stage arena, which is covered and has proper
lighting. Also, beginner competitions (1.10m or less) are scheduled on the first
day, and advanced competitions (1.50m or more) are scheduled on the last day.

The information for next winter's show is as follows:
Available stalls: 100
Number of riders: 100
Number of horses: 130
Number of requested Entries: 200
Number of competitions: 15

Venue:
- Main Stage arena: Covered (9AM-11PM)
- Highlands arena: Daylight Only (9AM-5PM)
- Sawdust arena: Daylight Only (9AM-5PM)
- Paddock1 has capacity for 10 riders and serves Main Stage
- Paddock2 has capacity for 6 riders and serves Main Stage
- Paddock3 has capacity for 8 riders and serves Main Stage, Highlands
- Paddock4 has capacity for 8 riders and serves Highlands, Sawdust
- Paddock5 has capacity for 9 riders and serves Sawdust
- Paddock6 has capacity for 7 riders and serves Sawdust

competitions:
- C_5_1.10m_Year_Olds  1.10m -  60 minutes
- C_6_1.25m_Year_Olds  1.25m -  90 minutes
- C_7_1.35m_Year_Olds  1.35m - 120 minutes
- C_0.8m_Jumpers       0.80m - 240 minutes
- C_1.0m_Jumpers       1.00m - 180 minutes
- C_1.10m_Jumpers      1.10m - 180 minutes
- C_1.20m_Jumpers      1.20m - 120 minutes
- C_1.30m_Jumpers      1.30m - 120 minutes
- C_1.40m_Jumpers      1.40m - 120 minutes
- C_1.20m_Derby        1.20m - 180 minutes
- C_1.35m_Derby        1.35m - 180 minutes
- C_1.45m_Derby        1.45m - 180 minutes
- C_1.40m_Open         1.40m - 120 minutes
- C_1.50m_Open         1.50m - 180 minutes
- C_1.60m_Grand_Prix   1.60m - 240 minutes
"""

import dataclasses
from absl import app
import numpy as np
from ortools.sat.python import cp_model


@dataclasses.dataclass(frozen=True)
class Arena:
    """Data for an arena."""

    id: str
    hours: str


@dataclasses.dataclass(frozen=True)
class Competition:
    """Data for a competition."""

    id: str
    height: float
    duration: int


@dataclasses.dataclass(frozen=True)
class HorseJumpingShowData:
    """Horse Jumping Show Data."""

    num_days: int
    competitions: list[Competition]
    arenas: list[Arena]


@dataclasses.dataclass(frozen=True)
class ScheduledCompetition:
    """Horse Jumping Show Schedule."""

    completion: str
    day: int
    arena: str
    start_time: str
    end_time: str


def generate_horse_jumping_show_data() -> HorseJumpingShowData:
    """Generates the horse jumping show data."""
    arenas = [
        Arena(id="Main Stage", hours="9AM-9PM"),
        Arena(id="Highlands", hours="9AM-5PM"),
        Arena(id="Sawdust", hours="9AM-5PM"),
    ]
    competitions = [
        Competition(id="C_5_1.10m_Year_Olds", height=1.1, duration=60),
        Competition(id="C_6_1.25m_Year_Olds", height=1.25, duration=90),
        Competition(id="C_7_1.35m_Year_Olds", height=1.35, duration=120),
        Competition(id="C_0.8m_Jumpers", height=0.8, duration=240),
        Competition(id="C_1.0m_Jumpers", height=1.0, duration=180),
        Competition(id="C_1.10m_Jumpers", height=1.10, duration=180),
        Competition(id="C_1.20m_Jumpers", height=1.20, duration=120),
        Competition(id="C_1.30m_Jumpers", height=1.30, duration=120),
        Competition(id="C_1.40m_Jumpers", height=1.40, duration=120),
        Competition(id="C_1.20m_Derby", height=1.20, duration=180),
        Competition(id="C_1.35m_Derby", height=1.35, duration=180),
        Competition(id="C_1.45m_Derby", height=1.45, duration=180),
        Competition(id="C_1.40m_Open", height=1.40, duration=120),
        Competition(id="C_1.50m_Open", height=1.50, duration=180),
        Competition(id="C_1.60m_Grand_Prix", height=1.60, duration=240),
    ]
    return HorseJumpingShowData(num_days=3, competitions=competitions, arenas=arenas)


def solve() -> list[ScheduledCompetition]:
    """Solves the horse jumping show problem."""
    data = generate_horse_jumping_show_data()
    num_days = data.num_days
    competitions = data.competitions
    arenas = data.arenas
    day_index = list(range(num_days))

    # Time parser.
    def parse_time(t_str):
        hour = int(t_str[:-2])
        if "PM" in t_str and hour != 12:
            hour += 12
        if "AM" in t_str and hour == 12:
            hour = 0
        return hour * 60

    # Schedule time intervals for each arena.
    schedule_interval_by_arena = {}
    for arena in arenas:
        start_h_str, end_h_str = arena.hours.split("-")
        start_time = parse_time(start_h_str)
        end_time = parse_time(end_h_str)
        schedule_interval_by_arena[arena.id] = (start_time, end_time)

    # Map time to 30-minute intervals and back.
    time_slot_size = 30

    def time_to_slot(time_in_minutes: int):
        return time_in_minutes // time_slot_size

    def slot_to_time(slot_index: int):
        return slot_index * time_slot_size

    # --- Model Creation ---
    model = cp_model.CpModel()

    # --- Variables ---
    # Competition scheduling variables per arena and day.
    competition_assignments = np.empty(
        (len(competitions), len(arenas), num_days), dtype=object
    )
    for c, comp in enumerate(competitions):
        for a, arena in enumerate(arenas):
            for d in day_index:
                competition_assignments[c, a, d] = model.new_bool_var(
                    f"competition_scheduled_{comp.id}_{arena.id}_{d}"
                )
    # Time intervals and start times for each competition. We model time steps
    # 0,1,2,... to represent the start times in 30 minutes intervals, as opposed
    # to represent the start times in minutes.
    competition_start_times = np.empty(
        (len(competitions), len(arenas), num_days), dtype=object
    )
    competition_intervals = np.empty(
        (len(competitions), len(arenas), num_days), dtype=object
    )
    for c, comp in enumerate(competitions):
        for a, arena in enumerate(arenas):
            earliest_start_time, latest_end_time = schedule_interval_by_arena[arena.id]
            latest_start_time = latest_end_time - comp.duration
            for d in day_index:
                competition_start_times[c, a, d] = model.new_int_var(
                    time_to_slot(earliest_start_time),
                    time_to_slot(latest_start_time),
                    f"start_time_{comp.id}_{arena.id}_{d}",
                )
                competition_intervals[c, a, d] = (
                    model.new_optional_fixed_size_interval_var(
                        competition_start_times[c, a, d],
                        time_to_slot(comp.duration),
                        competition_assignments[c, a, d],
                        f"task_{comp.id}_{arena.id}_{d}",
                    )
                )

    # --- Constraints ---
    # Every competition must be scheduled, enforcing that beginner competitions
    # are on day 1, and advanced competitions are on day 3.
    for c, comp in enumerate(competitions):
        model.add(np.sum(competition_assignments[c, :, :]) == 1)
        # Beginner competitions are on the first day.
        if comp.height <= 1.10:
            beginners_day = 0
            model.add(np.sum(competition_assignments[c, :, beginners_day]) == 1)
        # Advanced competitions are on the last day.
        if comp.height >= 1.50:
            advanced_day = num_days - 1
            model.add(np.sum(competition_assignments[c, :, advanced_day]) == 1)

    # Competitions scheduled on the same arena and on the same day can't overlap.
    for a, _ in enumerate(arenas):
        for day in range(num_days):
            model.add_no_overlap(competition_intervals[:, a, day])

    # Start times should be scattered across the day.
    for a, _ in enumerate(arenas):
        for day in day_index:
            model.add_all_different(competition_start_times[:, a, day])

    # --- Objective ---
    model.maximize(np.sum(competition_start_times))

    # --- Solve ---
    solver = cp_model.CpSolver()
    solver.parameters.max_time_in_seconds = 30.0
    solver.parameters.log_search_progress = True
    solver.parameters.num_workers = 16
    status = solver.solve(model)

    # --- Print Solution ---
    if status == cp_model.OPTIMAL or status == cp_model.FEASIBLE:
        schedule = []
        for day in range(num_days):
            for c, comp in enumerate(competitions):
                for a, arena in enumerate(arenas):
                    if solver.value(competition_assignments[c, a, day]):
                        start_time_minutes = slot_to_time(
                            solver.value(competition_start_times[c, a, day])
                        )
                        start_h, start_m = divmod(start_time_minutes, 60)
                        end_h, end_m = divmod(start_time_minutes + comp.duration, 60)
                        schedule.append(
                            ScheduledCompetition(
                                completion=comp.id,
                                day=day + 1,
                                arena=arena.id,
                                start_time=f"{start_h:02d}:{start_m:02d}",
                                end_time=f"{end_h:02d}:{end_m:02d}",
                            )
                        )
        # Sort and print schedule for readability.
        schedule.sort(key=lambda x: (x.day, x.start_time))
        print("Schedule:")
        for item in schedule:
            print(
                f"Day {item.day}:  {item.completion} in {item.arena} from"
                f" {item.start_time} to {item.end_time}."
            )
        return schedule
    elif status == cp_model.INFEASIBLE:
        print("Problem is infeasible.")
    else:
        print("No solution found.")
    # Return an empty schedule if no solution is found.
    return []


def main(_):
    solve()


if __name__ == "__main__":
    app.run(main)
