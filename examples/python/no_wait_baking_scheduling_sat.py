#!/usr/bin/env python3
# Copyright 2010-2024 Google LLC
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

"""Scheduling cooking tasks in a bakery using no-wait jobshop scheduling.

We are scheduling a full day of baking:
  - Work starts at 4am.
  - The shop closes at 10PM.
  - Durations are in minutes. Time starts at midnight.
"""

import collections
from typing import Sequence
from absl import app
from absl import flags

from google.protobuf import text_format
from ortools.sat.python import cp_model

_PARAMS = flags.DEFINE_string(
    "params", "num_search_workers:16, max_time_in_seconds:30", "Sat solver parameters."
)
_PROTO_FILE = flags.DEFINE_string(
    "proto_file", "", "If not empty, output the proto to this file."
)

# Recipes
CROISSANT = "croissant"
APPLE_PIE = "apple pie"
BRIOCHE = "brioche"
CHOCOLATE_CAKE = "chocolate cake"

# Skills
BAKING = "baking"
PROOFING = "proofing"
COOKING = "cooking"
COOLING = "cooling"
DECORATING = "decorating"
DISPLAY = "display"


class Task(object):
    """A unit baking task.

    - Simple baking tasks have a fixed duration. They are performed by workers.
    - Waiting/cooling/proofing tasks have a min and a max duration.
      They are performed by machine or they use space resources.
    """

    def __init__(self, name, min_duration, max_duration):
        self.name = name
        self.min_duration = min_duration
        self.max_duration = max_duration


class Skill(object):
    """The skill of a worker or the capability of a machine."""

    def __init__(self, name, efficiency):
        self.name = name
        # Efficiency is currently not used.
        self.efficiency = efficiency


class Recipe(object):
    """A recipe is a sequence of cooking tasks."""

    def __init__(self, name):
        self.name = name
        self.tasks = []

    def add_task(self, resource_name, min_duration, max_duration):
        self.tasks.append(Task(resource_name, min_duration, max_duration))
        return self


class Resource(object):
    """A resource is a worker, a machine, or just some space for cakes to rest.

    - Workers have a capacity of 1 and can have variable efficiency.
    - Machines and spaces have a capacity greater or equal to one, but the
      efficiency is fixed to 100.

      For a worker with efficiency k and a task of duration t, the resulting
      work will have a duration `ceil(t * k)`.
    """

    def __init__(self, name, capacity):
        self.name = name
        self.capacity = capacity
        self.skills = []

    def add_skill(self, skill_name, efficiency):
        self.skills.append(Skill(skill_name, efficiency))
        return self


class Order(object):
    """An order is a recipe that should be delivered at a given due date."""

    def __init__(self, unique_id, recipe_name, due_date, quantity):
        """Builds an order.

        Args:
          unique_id: A unique identifier for the order. Used to display the result.
          recipe_name: The name of the recipe. It must match one of the recipes.
          due_date: The due date in minutes since midnight.
          quantity: How many cakes to prepare.
        """
        self.unique_id = unique_id
        self.recipe_name = recipe_name
        self.due_date = due_date
        self.quantity = quantity


def set_up_data():
    """Set up the bakery problem data."""

    # Recipes.
    croissant_recipe = Recipe(CROISSANT)
    croissant_recipe.add_task(BAKING, 15, 15)
    croissant_recipe.add_task(PROOFING, 60, 90)
    croissant_recipe.add_task(COOKING, 20, 20)
    croissant_recipe.add_task(DISPLAY, 5, 5 * 60)

    apple_pie_recipe = Recipe(APPLE_PIE)
    apple_pie_recipe.add_task(BAKING, 25, 25)
    apple_pie_recipe.add_task(PROOFING, 15, 60)
    apple_pie_recipe.add_task(COOKING, 30, 30)
    apple_pie_recipe.add_task(DECORATING, 5, 5)
    apple_pie_recipe.add_task(DISPLAY, 5, 5 * 60)

    brioche_recipe = Recipe(BRIOCHE)
    brioche_recipe.add_task(BAKING, 20, 20)
    brioche_recipe.add_task(PROOFING, 60, 90)
    brioche_recipe.add_task(COOKING, 30, 30)
    brioche_recipe.add_task(DISPLAY, 5, 5 * 60)

    chocolate_cake_recipe = Recipe(CHOCOLATE_CAKE)
    chocolate_cake_recipe.add_task(BAKING, 15, 15)
    chocolate_cake_recipe.add_task(COOKING, 25, 25)
    chocolate_cake_recipe.add_task(DECORATING, 15, 15)
    chocolate_cake_recipe.add_task(DISPLAY, 5, 5 * 60)
    recipes = [
        croissant_recipe,
        apple_pie_recipe,
        brioche_recipe,
        chocolate_cake_recipe,
    ]

    # Resources.
    baker1 = Resource("baker1", 1).add_skill(BAKING, 1.0)
    baker2 = Resource("baker2", 1).add_skill(BAKING, 1.0)
    decorator1 = Resource("decorator1", 1).add_skill(DECORATING, 1.0)
    waiting_space = Resource("waiting_space", 4).add_skill(PROOFING, 1.0)
    oven = Resource("oven", 4).add_skill(COOKING, 1.0)
    display_space = Resource("display_space", 12).add_skill(DISPLAY, 1.0)
    resources = [baker1, baker2, decorator1, waiting_space, oven, display_space]

    # Orders
    croissant_7am = Order("croissant_7am", CROISSANT, 7 * 60, 3)
    croissant_8am = Order("croissant_8am", CROISSANT, 8 * 60, 3)
    croissant_9am = Order("croissant_9am", CROISSANT, 9 * 60, 2)
    croissant_10am = Order("croissant_10am", CROISSANT, 10 * 60, 1)
    croissant_11am = Order("croissant_11am", CROISSANT, 11 * 60, 1)
    brioche_10am = Order("brioche_10am", BRIOCHE, 10 * 60, 8)
    brioche_12pm = Order("brioche_12pm", BRIOCHE, 12 * 60, 8)
    apple_pie_1pm = Order("apple_pie_1pm", APPLE_PIE, 13 * 60, 10)
    chocolate_4pm = Order("chocolate_4pm", CHOCOLATE_CAKE, 16 * 60, 10)
    orders = [
        croissant_7am,
        croissant_8am,
        croissant_9am,
        croissant_10am,
        croissant_11am,
        brioche_10am,
        brioche_12pm,
        apple_pie_1pm,
        chocolate_4pm,
    ]

    return recipes, resources, orders


def solve_with_cp_sat(recipes, resources, orders):
    """Build the optimization model, and solve the problem."""

    model = cp_model.CpModel()
    horizon = 22 * 60  # 10PM.
    start_work = 4 * 60  # 4am.

    # Parse recipes.
    recipe_by_name = {}
    for recipe in recipes:
        recipe_by_name[recipe.name] = recipe

    # Parse resources.
    resource_by_name = {}
    resource_list_by_skill_name = collections.defaultdict(list)
    for resource in resources:
        resource_by_name[resource.name] = resource
        for skill in resource.skills:
            resource_list_by_skill_name[skill.name].append(resource)

    # Parse orders and create one optional copy per eligible resource and per
    # task.
    interval_list_by_resource_name = collections.defaultdict(list)
    orders_sequence_of_events = collections.defaultdict(list)
    sorted_orders = []
    tardiness_vars = []
    for order in orders:
        for batch in range(order.quantity):
            order_id = f"{order.unique_id}_{batch}"
            sorted_orders.append(order_id)
            previous_end = None
            due_date = order.due_date
            recipe = recipe_by_name[order.recipe_name]
            for task in recipe.tasks:
                skill_name = task.name
                suffix = f"_{order.unique_id}_batch{batch}_{skill_name}"

                start = None
                if previous_end is None:
                    start = model.new_int_var(start_work, horizon, f"start{suffix}")
                    orders_sequence_of_events[order_id].append(
                        (start, f"start{suffix}")
                    )
                else:
                    start = previous_end

                size = model.new_int_var(
                    task.min_duration, task.max_duration, f"size{suffix}"
                )
                end = None
                if task == recipe.tasks[-1]:
                    # The order must end after the due_date. Ideally, exactly at the
                    # due_date.
                    tardiness = model.new_int_var(0, horizon - due_date, f"end{suffix}")
                    end = tardiness + due_date

                    # Store the end_var for the objective.
                    tardiness_vars.append(tardiness)
                else:
                    end = model.new_int_var(start_work, horizon, f"end{suffix}")
                orders_sequence_of_events[order_id].append((end, f"end{suffix}"))
                previous_end = end

                # Per resource copy.
                presence_literals = []
                for resource in resource_list_by_skill_name[skill_name]:
                    presence = model.new_bool_var(f"presence{suffix}_{resource.name}")
                    copy = model.new_optional_interval_var(
                        start, size, end, presence, f"interval{suffix}_{resource.name}"
                    )
                    interval_list_by_resource_name[resource.name].append(copy)
                    presence_literals.append(presence)

                # Only one copy will be performed.
                model.add_exactly_one(presence_literals)

    # Create resource constraints.
    for resource in resources:
        intervals = interval_list_by_resource_name[resource.name]
        if resource.capacity == 1:
            model.add_no_overlap(intervals)
        else:
            model.add_cumulative(intervals, [1] * len(intervals), resource.capacity)

    # The objective is to minimize the sum of the tardiness values of each jobs.
    # The tardiness is difference between the end time of an order and its
    # due date.
    model.minimize(sum(tardiness_vars))

    # Solve model.
    solver = cp_model.CpSolver()
    if _PARAMS.value:
        text_format.Parse(_PARAMS.value, solver.parameters)
    solver.parameters.log_search_progress = True
    status = solver.solve(model)

    if status == cp_model.OPTIMAL or status == cp_model.FEASIBLE:
        for order_id in sorted_orders:
            print(f"{order_id}:")
            for time_expr, event_id in orders_sequence_of_events[order_id]:
                time = solver.value(time_expr)
                print(f"  {event_id} at {time // 60}:{time % 60:02}")


def main(argv: Sequence[str]) -> None:
    if len(argv) > 1:
        raise app.UsageError("Too many command-line arguments.")

    recipes, resources, orders = set_up_data()
    solve_with_cp_sat(recipes, resources, orders)


if __name__ == "__main__":
    app.run(main)
