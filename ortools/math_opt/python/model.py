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

"""A solver independent library for modeling optimization problems.

Example use to model the optimization problem:
   max 2.0 * x + y
   s.t. x + y <= 1.5
            x in {0.0, 1.0}
            y in [0.0, 2.5]

  model = mathopt.Model(name='my_model')
  x = model.add_binary_variable(name='x')
  y = model.add_variable(lb=0.0, ub=2.5, name='y')
  # We can directly use linear combinations of variables ...
  model.add_linear_constraint(x + y <= 1.5, name='c')
  # ... or build them incrementally.
  objective_expression = 0
  objective_expression += 2 * x
  objective_expression += y
  model.maximize(objective_expression)

  # May raise a RuntimeError on invalid input or internal solver errors.
  result = mathopt.solve(model, mathopt.SolverType.GSCIP)

  if result.termination.reason not in (mathopt.TerminationReason.OPTIMAL,
                                       mathopt.TerminationReason.FEASIBLE):
    raise RuntimeError(f'model failed to solve: {result.termination}')

  print(f'Objective value: {result.objective_value()}')
  print(f'Value for variable x: {result.variable_values()[x]}')
"""

import math
from typing import Iterator, Optional, Tuple, Union

from ortools.math_opt import model_pb2
from ortools.math_opt import model_update_pb2
from ortools.math_opt.elemental.python import cpp_elemental
from ortools.math_opt.elemental.python import enums
from ortools.math_opt.python import from_model
from ortools.math_opt.python import indicator_constraints
from ortools.math_opt.python import linear_constraints as linear_constraints_mod
from ortools.math_opt.python import normalized_inequality
from ortools.math_opt.python import objectives
from ortools.math_opt.python import quadratic_constraints
from ortools.math_opt.python import variables as variables_mod
from ortools.math_opt.python.elemental import elemental


class UpdateTracker:
    """Tracks updates to an optimization model from a ModelStorage.

    Do not instantiate directly, instead create through
    ModelStorage.add_update_tracker().

    Querying an UpdateTracker after calling Model.remove_update_tracker will
    result in a model_storage.UsedUpdateTrackerAfterRemovalError.

    Example:
      mod = Model()
      x = mod.add_variable(0.0, 1.0, True, 'x')
      y = mod.add_variable(0.0, 1.0, True, 'y')
      tracker = mod.add_update_tracker()
      mod.set_variable_ub(x, 3.0)
      tracker.export_update()
        => "variable_updates: {upper_bounds: {ids: [0], values[3.0] }"
      mod.set_variable_ub(y, 2.0)
      tracker.export_update()
        => "variable_updates: {upper_bounds: {ids: [0, 1], values[3.0, 2.0] }"
      tracker.advance_checkpoint()
      tracker.export_update()
        => None
      mod.set_variable_ub(y, 4.0)
      tracker.export_update()
        => "variable_updates: {upper_bounds: {ids: [1], values[4.0] }"
      tracker.advance_checkpoint()
      mod.remove_update_tracker(tracker)
    """

    def __init__(
        self,
        diff_id: int,
        elem: elemental.Elemental,
    ):
        """Do not invoke directly, use Model.add_update_tracker() instead."""
        self._diff_id = diff_id
        self._elemental = elem

    def export_update(
        self, *, remove_names: bool = False
    ) -> Optional[model_update_pb2.ModelUpdateProto]:
        """Returns changes to the model since last call to checkpoint/creation."""
        return self._elemental.export_model_update(
            self._diff_id, remove_names=remove_names
        )

    def advance_checkpoint(self) -> None:
        """Track changes to the model only after this function call."""
        return self._elemental.advance_diff(self._diff_id)

    @property
    def diff_id(self) -> int:
        return self._diff_id


class Model:
    """An optimization model.

    The objective function of the model can be linear or quadratic, and some
    solvers can only handle linear objective functions. For this reason Model has
    three versions of all objective setting functions:
      * A generic one (e.g. maximize()), which accepts linear or quadratic
        expressions,
      * a quadratic version (e.g. maximize_quadratic_objective()), which also
        accepts linear or quadratic expressions and can be used to signal a
        quadratic objective is possible, and
      * a linear version (e.g. maximize_linear_objective()), which only accepts
        linear expressions and can be used to avoid solve time errors for solvers
        that do not accept quadratic objectives.

    Attributes:
      name: A description of the problem, can be empty.
      objective: A function to maximize or minimize.
      storage: Implementation detail, do not access directly.
      _variable_ids: Maps variable ids to Variable objects.
      _linear_constraint_ids: Maps linear constraint ids to LinearConstraint
        objects.
    """

    __slots__ = ("_elemental",)

    def __init__(
        self,
        *,
        name: str = "",  # TODO(b/371236599): rename to model_name
        primary_objective_name: str = "",
    ) -> None:
        self._elemental: elemental.Elemental = cpp_elemental.CppElemental(
            model_name=name, primary_objective_name=primary_objective_name
        )

    @property
    def name(self) -> str:
        return self._elemental.model_name

    ##############################################################################
    # Variables
    ##############################################################################

    def add_variable(
        self,
        *,
        lb: float = -math.inf,
        ub: float = math.inf,
        is_integer: bool = False,
        name: str = "",
    ) -> variables_mod.Variable:
        """Adds a decision variable to the optimization model.

        Args:
          lb: The new variable must take at least this value (a lower bound).
          ub: The new variable must be at most this value (an upper bound).
          is_integer: Indicates if the variable can only take integer values
            (otherwise, the variable can take any continuous value).
          name: For debugging purposes only, but nonempty names must be distinct.

        Returns:
          A reference to the new decision variable.
        """

        variable_id = self._elemental.add_element(enums.ElementType.VARIABLE, name)
        result = variables_mod.Variable(self._elemental, variable_id)
        result.lower_bound = lb
        result.upper_bound = ub
        result.integer = is_integer
        return result

    def add_integer_variable(
        self, *, lb: float = -math.inf, ub: float = math.inf, name: str = ""
    ) -> variables_mod.Variable:
        return self.add_variable(lb=lb, ub=ub, is_integer=True, name=name)

    def add_binary_variable(self, *, name: str = "") -> variables_mod.Variable:
        return self.add_variable(lb=0.0, ub=1.0, is_integer=True, name=name)

    def get_variable(
        self, var_id: int, *, validate: bool = True
    ) -> variables_mod.Variable:
        """Returns the Variable for the id var_id, or raises KeyError."""
        if validate and not self._elemental.element_exists(
            enums.ElementType.VARIABLE, var_id
        ):
            raise KeyError(f"Variable does not exist with id {var_id}.")
        return variables_mod.Variable(self._elemental, var_id)

    def has_variable(self, var_id: int) -> bool:
        """Returns true if a Variable with this id is in the model."""
        return self._elemental.element_exists(enums.ElementType.VARIABLE, var_id)

    def get_num_variables(self) -> int:
        """Returns the number of variables in the model."""
        return self._elemental.get_num_elements(enums.ElementType.VARIABLE)

    def get_next_variable_id(self) -> int:
        """Returns the id of the next variable created in the model."""
        return self._elemental.get_next_element_id(enums.ElementType.VARIABLE)

    def ensure_next_variable_id_at_least(self, var_id: int) -> None:
        """If the next variable id would be less than `var_id`, sets it to `var_id`."""
        self._elemental.ensure_next_element_id_at_least(
            enums.ElementType.VARIABLE, var_id
        )

    def delete_variable(self, var: variables_mod.Variable) -> None:
        """Removes this variable from the model."""
        self.check_compatible(var)
        if not self._elemental.delete_element(enums.ElementType.VARIABLE, var.id):
            raise ValueError(f"Variable with id {var.id} was not in the model.")

    def variables(self) -> Iterator[variables_mod.Variable]:
        """Yields the variables in the order of creation."""
        var_ids = self._elemental.get_elements(enums.ElementType.VARIABLE)
        var_ids.sort()
        for var_id in var_ids:
            yield variables_mod.Variable(self._elemental, int(var_id))

    ##############################################################################
    # Objective
    ##############################################################################

    @property
    def objective(self) -> objectives.Objective:
        return objectives.PrimaryObjective(self._elemental)

    def maximize(self, obj: variables_mod.QuadraticTypes) -> None:
        """Sets the objective to maximize the provided expression `obj`."""
        self.set_objective(obj, is_maximize=True)

    def maximize_linear_objective(self, obj: variables_mod.LinearTypes) -> None:
        """Sets the objective to maximize the provided linear expression `obj`."""
        self.set_linear_objective(obj, is_maximize=True)

    def maximize_quadratic_objective(self, obj: variables_mod.QuadraticTypes) -> None:
        """Sets the objective to maximize the provided quadratic expression `obj`."""
        self.set_quadratic_objective(obj, is_maximize=True)

    def minimize(self, obj: variables_mod.QuadraticTypes) -> None:
        """Sets the objective to minimize the provided expression `obj`."""
        self.set_objective(obj, is_maximize=False)

    def minimize_linear_objective(self, obj: variables_mod.LinearTypes) -> None:
        """Sets the objective to minimize the provided linear expression `obj`."""
        self.set_linear_objective(obj, is_maximize=False)

    def minimize_quadratic_objective(self, obj: variables_mod.QuadraticTypes) -> None:
        """Sets the objective to minimize the provided quadratic expression `obj`."""
        self.set_quadratic_objective(obj, is_maximize=False)

    def set_objective(
        self, obj: variables_mod.QuadraticTypes, *, is_maximize: bool
    ) -> None:
        """Sets the objective to optimize the provided expression `obj`."""
        self.objective.set_to_expression(obj)
        self.objective.is_maximize = is_maximize

    def set_linear_objective(
        self, obj: variables_mod.LinearTypes, *, is_maximize: bool
    ) -> None:
        """Sets the objective to optimize the provided linear expression `obj`."""
        self.objective.set_to_linear_expression(obj)
        self.objective.is_maximize = is_maximize

    def set_quadratic_objective(
        self, obj: variables_mod.QuadraticTypes, *, is_maximize: bool
    ) -> None:
        """Sets the objective to optimize the provided quadratic expression `obj`."""
        self.objective.set_to_quadratic_expression(obj)
        self.objective.is_maximize = is_maximize

    def linear_objective_terms(self) -> Iterator[variables_mod.LinearTerm]:
        """Yields variable coefficient pairs for variables with nonzero objective coefficient in undefined order."""
        yield from self.objective.linear_terms()

    def quadratic_objective_terms(self) -> Iterator[variables_mod.QuadraticTerm]:
        """Yields the quadratic terms with nonzero objective coefficient in undefined order."""
        yield from self.objective.quadratic_terms()

    ##############################################################################
    # Auxiliary Objectives
    ##############################################################################

    def add_auxiliary_objective(
        self,
        *,
        priority: int,
        name: str = "",
        expr: Optional[variables_mod.LinearTypes] = None,
        is_maximize: bool = False,
    ) -> objectives.AuxiliaryObjective:
        """Adds an additional objective to the model."""
        obj_id = self._elemental.add_element(
            enums.ElementType.AUXILIARY_OBJECTIVE, name
        )
        self._elemental.set_attr(
            enums.IntAttr1.AUXILIARY_OBJECTIVE_PRIORITY, (obj_id,), priority
        )
        result = objectives.AuxiliaryObjective(self._elemental, obj_id)
        if expr is not None:
            result.set_to_linear_expression(expr)
        result.is_maximize = is_maximize
        return result

    def add_maximization_objective(
        self, expr: variables_mod.LinearTypes, *, priority: int, name: str = ""
    ) -> objectives.AuxiliaryObjective:
        """Adds an additional objective to the model that is maximizaition."""
        result = self.add_auxiliary_objective(
            priority=priority, name=name, expr=expr, is_maximize=True
        )
        return result

    def add_minimization_objective(
        self, expr: variables_mod.LinearTypes, *, priority: int, name: str = ""
    ) -> objectives.AuxiliaryObjective:
        """Adds an additional objective to the model that is minimizaition."""
        result = self.add_auxiliary_objective(
            priority=priority, name=name, expr=expr, is_maximize=False
        )
        return result

    def delete_auxiliary_objective(self, obj: objectives.AuxiliaryObjective) -> None:
        """Removes an auxiliary objective from the model."""
        self.check_compatible(obj)
        if not self._elemental.delete_element(
            enums.ElementType.AUXILIARY_OBJECTIVE, obj.id
        ):
            raise ValueError(
                f"Auxiliary objective with id {obj.id} is not in the model."
            )

    def has_auxiliary_objective(self, obj_id: int) -> bool:
        """Returns true if the model has an auxiliary objective with id `obj_id`."""
        return self._elemental.element_exists(
            enums.ElementType.AUXILIARY_OBJECTIVE, obj_id
        )

    def next_auxiliary_objective_id(self) -> int:
        """Returns the id of the next auxiliary objective added to the model."""
        return self._elemental.get_next_element_id(
            enums.ElementType.AUXILIARY_OBJECTIVE
        )

    def num_auxiliary_objectives(self) -> int:
        """Returns the number of auxiliary objectives in this model."""
        return self._elemental.get_num_elements(enums.ElementType.AUXILIARY_OBJECTIVE)

    def ensure_next_auxiliary_objective_id_at_least(self, obj_id: int) -> None:
        """If the next auxiliary objective id would be less than `obj_id`, sets it to `obj_id`."""
        self._elemental.ensure_next_element_id_at_least(
            enums.ElementType.AUXILIARY_OBJECTIVE, obj_id
        )

    def get_auxiliary_objective(
        self, obj_id: int, *, validate: bool = True
    ) -> objectives.AuxiliaryObjective:
        """Returns the auxiliary objective with this id.

        If there is no objective with this id, an exception is thrown if validate is
        true, and an invalid AuxiliaryObjective is returned if validate is false
        (later interactions with this object will cause unpredictable errors). Only
        set validate=False if there is a known performance problem.

        Args:
          obj_id: The id of the auxiliary objective to look for.
          validate: Set to false for more speed, but fails to raise an exception if
            the objective is missing.

        Raises:
          KeyError: If `validate` is True and there is no objective with this id.
        """
        if validate and not self.has_auxiliary_objective(obj_id):
            raise KeyError(f"Model has no auxiliary objective with id {obj_id}")
        return objectives.AuxiliaryObjective(self._elemental, obj_id)

    def auxiliary_objectives(self) -> Iterator[objectives.AuxiliaryObjective]:
        """Returns the auxiliary objectives in the model in the order of creation."""
        ids = self._elemental.get_elements(enums.ElementType.AUXILIARY_OBJECTIVE)
        ids.sort()
        for aux_obj_id in ids:
            yield objectives.AuxiliaryObjective(self._elemental, int(aux_obj_id))

    ##############################################################################
    # Linear Constraints
    ##############################################################################

    # TODO(b/227214976): Update the note below and link to pytype bug number.
    # Note: bounded_expr's type includes bool only as a workaround to a pytype
    # issue. Passing a bool for bounded_expr will raise an error in runtime.
    def add_linear_constraint(
        self,
        bounded_expr: Optional[Union[bool, variables_mod.BoundedLinearTypes]] = None,
        *,
        lb: Optional[float] = None,
        ub: Optional[float] = None,
        expr: Optional[variables_mod.LinearTypes] = None,
        name: str = "",
    ) -> linear_constraints_mod.LinearConstraint:
        """Adds a linear constraint to the optimization model.

        The simplest way to specify the constraint is by passing a one-sided or
        two-sided linear inequality as in:
          * add_linear_constraint(x + y + 1.0 <= 2.0),
          * add_linear_constraint(x + y >= 2.0), or
          * add_linear_constraint((1.0 <= x + y) <= 2.0).

        Note the extra parenthesis for two-sided linear inequalities, which is
        required due to some language limitations (see
        https://peps.python.org/pep-0335/ and https://peps.python.org/pep-0535/).
        If the parenthesis are omitted, a TypeError will be raised explaining the
        issue (if this error was not raised the first inequality would have been
        silently ignored because of the noted language limitations).

        The second way to specify the constraint is by setting lb, ub, and/or expr
        as in:
          * add_linear_constraint(expr=x + y + 1.0, ub=2.0),
          * add_linear_constraint(expr=x + y, lb=2.0),
          * add_linear_constraint(expr=x + y, lb=1.0, ub=2.0), or
          * add_linear_constraint(lb=1.0).
        Omitting lb is equivalent to setting it to -math.inf and omiting ub is
        equivalent to setting it to math.inf.

        These two alternatives are exclusive and a combined call like:
          * add_linear_constraint(x + y <= 2.0, lb=1.0), or
          * add_linear_constraint(x + y <= 2.0, ub=math.inf)
        will raise a ValueError. A ValueError is also raised if expr's offset is
        infinite.

        Args:
          bounded_expr: a linear inequality describing the constraint. Cannot be
            specified together with lb, ub, or expr.
          lb: The constraint's lower bound if bounded_expr is omitted (if both
            bounder_expr and lb are omitted, the lower bound is -math.inf).
          ub: The constraint's upper bound if bounded_expr is omitted (if both
            bounder_expr and ub are omitted, the upper bound is math.inf).
          expr: The constraint's linear expression if bounded_expr is omitted.
          name: For debugging purposes only, but nonempty names must be distinct.

        Returns:
          A reference to the new linear constraint.
        """
        norm_ineq = normalized_inequality.as_normalized_linear_inequality(
            bounded_expr, lb=lb, ub=ub, expr=expr
        )
        lin_con_id = self._elemental.add_element(
            enums.ElementType.LINEAR_CONSTRAINT, name
        )

        result = linear_constraints_mod.LinearConstraint(self._elemental, lin_con_id)
        result.lower_bound = norm_ineq.lb
        result.upper_bound = norm_ineq.ub
        for var, coefficient in norm_ineq.coefficients.items():
            result.set_coefficient(var, coefficient)
        return result

    def has_linear_constraint(self, con_id: int) -> bool:
        """Returns true if a linear constraint with this id is in the model."""
        return self._elemental.element_exists(
            enums.ElementType.LINEAR_CONSTRAINT, con_id
        )

    def get_num_linear_constraints(self) -> int:
        """Returns the number of linear constraints in the model."""
        return self._elemental.get_num_elements(enums.ElementType.LINEAR_CONSTRAINT)

    def get_next_linear_constraint_id(self) -> int:
        """Returns the id of the next linear constraint created in the model."""
        return self._elemental.get_next_element_id(enums.ElementType.LINEAR_CONSTRAINT)

    def ensure_next_linear_constraint_id_at_least(self, con_id: int) -> None:
        """If the next linear constraint id would be less than `con_id`, sets it to `con_id`."""
        self._elemental.ensure_next_element_id_at_least(
            enums.ElementType.LINEAR_CONSTRAINT, con_id
        )

    def get_linear_constraint(
        self, con_id: int, *, validate: bool = True
    ) -> linear_constraints_mod.LinearConstraint:
        """Returns the LinearConstraint for the id con_id."""
        if validate and not self._elemental.element_exists(
            enums.ElementType.LINEAR_CONSTRAINT, con_id
        ):
            raise KeyError(f"Linear constraint does not exist with id {con_id}.")
        return linear_constraints_mod.LinearConstraint(self._elemental, con_id)

    def delete_linear_constraint(
        self, lin_con: linear_constraints_mod.LinearConstraint
    ) -> None:
        self.check_compatible(lin_con)
        if not self._elemental.delete_element(
            enums.ElementType.LINEAR_CONSTRAINT, lin_con.id
        ):
            raise ValueError(
                f"Linear constraint with id {lin_con.id} was not in the model."
            )

    def linear_constraints(
        self,
    ) -> Iterator[linear_constraints_mod.LinearConstraint]:
        """Yields the linear constraints in the order of creation."""
        lin_con_ids = self._elemental.get_elements(enums.ElementType.LINEAR_CONSTRAINT)
        lin_con_ids.sort()
        for lin_con_id in lin_con_ids:
            yield linear_constraints_mod.LinearConstraint(
                self._elemental, int(lin_con_id)
            )

    def row_nonzeros(
        self, lin_con: linear_constraints_mod.LinearConstraint
    ) -> Iterator[variables_mod.Variable]:
        """Yields the variables with nonzero coefficient for this linear constraint."""
        keys = self._elemental.slice_attr(
            enums.DoubleAttr2.LINEAR_CONSTRAINT_COEFFICIENT, 0, lin_con.id
        )
        for var_id in keys[:, 1]:
            yield variables_mod.Variable(self._elemental, int(var_id))

    def column_nonzeros(
        self, var: variables_mod.Variable
    ) -> Iterator[linear_constraints_mod.LinearConstraint]:
        """Yields the linear constraints with nonzero coefficient for this variable."""
        keys = self._elemental.slice_attr(
            enums.DoubleAttr2.LINEAR_CONSTRAINT_COEFFICIENT, 1, var.id
        )
        for lin_con_id in keys[:, 0]:
            yield linear_constraints_mod.LinearConstraint(
                self._elemental, int(lin_con_id)
            )

    def linear_constraint_matrix_entries(
        self,
    ) -> Iterator[linear_constraints_mod.LinearConstraintMatrixEntry]:
        """Yields the nonzero elements of the linear constraint matrix in undefined order."""
        keys = self._elemental.get_attr_non_defaults(
            enums.DoubleAttr2.LINEAR_CONSTRAINT_COEFFICIENT
        )
        coefs = self._elemental.get_attrs(
            enums.DoubleAttr2.LINEAR_CONSTRAINT_COEFFICIENT, keys
        )
        for i in range(len(keys)):
            yield linear_constraints_mod.LinearConstraintMatrixEntry(
                linear_constraint=linear_constraints_mod.LinearConstraint(
                    self._elemental, int(keys[i, 0])
                ),
                variable=variables_mod.Variable(self._elemental, int(keys[i, 1])),
                coefficient=float(coefs[i]),
            )

    ##############################################################################
    # Quadratic Constraints
    ##############################################################################

    def add_quadratic_constraint(
        self,
        bounded_expr: Optional[
            Union[
                bool,
                variables_mod.BoundedLinearTypes,
                variables_mod.BoundedQuadraticTypes,
            ]
        ] = None,
        *,
        lb: Optional[float] = None,
        ub: Optional[float] = None,
        expr: Optional[variables_mod.QuadraticTypes] = None,
        name: str = "",
    ) -> quadratic_constraints.QuadraticConstraint:
        """Adds a quadratic constraint to the optimization model.

        The simplest way to specify the constraint is by passing a one-sided or
        two-sided quadratic inequality as in:
          * add_quadratic_constraint(x * x + y + 1.0 <= 2.0),
          * add_quadratic_constraint(x * x + y >= 2.0), or
          * add_quadratic_constraint((1.0 <= x * x + y) <= 2.0).

        Note the extra parenthesis for two-sided linear inequalities, which is
        required due to some language limitations (see add_linear_constraint for
        details).

        The second way to specify the constraint is by setting lb, ub, and/or expr
        as in:
          * add_quadratic_constraint(expr=x * x + y + 1.0, ub=2.0),
          * add_quadratic_constraint(expr=x * x + y, lb=2.0),
          * add_quadratic_constraint(expr=x * x + y, lb=1.0, ub=2.0), or
          * add_quadratic_constraint(lb=1.0).
        Omitting lb is equivalent to setting it to -math.inf and omiting ub is
        equivalent to setting it to math.inf.

        These two alternatives are exclusive and a combined call like:
          * add_quadratic_constraint(x * x + y <= 2.0, lb=1.0), or
          * add_quadratic_constraint(x * x+ y <= 2.0, ub=math.inf)
        will raise a ValueError. A ValueError is also raised if expr's offset is
        infinite.

        Args:
          bounded_expr: a quadratic inequality describing the constraint. Cannot be
            specified together with lb, ub, or expr.
          lb: The constraint's lower bound if bounded_expr is omitted (if both
            bounder_expr and lb are omitted, the lower bound is -math.inf).
          ub: The constraint's upper bound if bounded_expr is omitted (if both
            bounder_expr and ub are omitted, the upper bound is math.inf).
          expr: The constraint's quadratic expression if bounded_expr is omitted.
          name: For debugging purposes only, but nonempty names must be distinct.

        Returns:
          A reference to the new quadratic constraint.
        """
        norm_quad = normalized_inequality.as_normalized_quadratic_inequality(
            bounded_expr, lb=lb, ub=ub, expr=expr
        )
        quad_con_id = self._elemental.add_element(
            enums.ElementType.QUADRATIC_CONSTRAINT, name
        )
        for var, coef in norm_quad.linear_coefficients.items():
            self._elemental.set_attr(
                enums.DoubleAttr2.QUADRATIC_CONSTRAINT_LINEAR_COEFFICIENT,
                (quad_con_id, var.id),
                coef,
            )
        for key, coef in norm_quad.quadratic_coefficients.items():
            self._elemental.set_attr(
                enums.SymmetricDoubleAttr3.QUADRATIC_CONSTRAINT_QUADRATIC_COEFFICIENT,
                (quad_con_id, key.first_var.id, key.second_var.id),
                coef,
            )
        if norm_quad.lb > -math.inf:
            self._elemental.set_attr(
                enums.DoubleAttr1.QUADRATIC_CONSTRAINT_LOWER_BOUND,
                (quad_con_id,),
                norm_quad.lb,
            )
        if norm_quad.ub < math.inf:
            self._elemental.set_attr(
                enums.DoubleAttr1.QUADRATIC_CONSTRAINT_UPPER_BOUND,
                (quad_con_id,),
                norm_quad.ub,
            )
        return quadratic_constraints.QuadraticConstraint(self._elemental, quad_con_id)

    def has_quadratic_constraint(self, con_id: int) -> bool:
        """Returns true if a quadratic constraint with this id is in the model."""
        return self._elemental.element_exists(
            enums.ElementType.QUADRATIC_CONSTRAINT, con_id
        )

    def get_num_quadratic_constraints(self) -> int:
        """Returns the number of quadratic constraints in the model."""
        return self._elemental.get_num_elements(enums.ElementType.QUADRATIC_CONSTRAINT)

    def get_next_quadratic_constraint_id(self) -> int:
        """Returns the id of the next quadratic constraint created in the model."""
        return self._elemental.get_next_element_id(
            enums.ElementType.QUADRATIC_CONSTRAINT
        )

    def ensure_next_quadratic_constraint_id_at_least(self, con_id: int) -> None:
        """If the next quadratic constraint id would be less than `con_id`, sets it to `con_id`."""
        self._elemental.ensure_next_element_id_at_least(
            enums.ElementType.QUADRATIC_CONSTRAINT, con_id
        )

    def get_quadratic_constraint(
        self, con_id: int, *, validate: bool = True
    ) -> quadratic_constraints.QuadraticConstraint:
        """Returns the constraint for the id, or raises KeyError if not in model."""
        if validate and not self._elemental.element_exists(
            enums.ElementType.QUADRATIC_CONSTRAINT, con_id
        ):
            raise KeyError(f"Quadratic constraint does not exist with id {con_id}.")
        return quadratic_constraints.QuadraticConstraint(self._elemental, con_id)

    def delete_quadratic_constraint(
        self, quad_con: quadratic_constraints.QuadraticConstraint
    ) -> None:
        """Deletes the constraint with id, or raises ValueError if not in model."""
        self.check_compatible(quad_con)
        if not self._elemental.delete_element(
            enums.ElementType.QUADRATIC_CONSTRAINT, quad_con.id
        ):
            raise ValueError(
                f"Quadratic constraint with id {quad_con.id} was not in the model."
            )

    def get_quadratic_constraints(
        self,
    ) -> Iterator[quadratic_constraints.QuadraticConstraint]:
        """Yields the quadratic constraints in the order of creation."""
        quad_con_ids = self._elemental.get_elements(
            enums.ElementType.QUADRATIC_CONSTRAINT
        )
        quad_con_ids.sort()
        for quad_con_id in quad_con_ids:
            yield quadratic_constraints.QuadraticConstraint(
                self._elemental, int(quad_con_id)
            )

    def quadratic_constraint_linear_nonzeros(
        self,
    ) -> Iterator[
        Tuple[
            quadratic_constraints.QuadraticConstraint,
            variables_mod.Variable,
            float,
        ]
    ]:
        """Yields the linear coefficients for all quadratic constraints in the model."""
        keys = self._elemental.get_attr_non_defaults(
            enums.DoubleAttr2.QUADRATIC_CONSTRAINT_LINEAR_COEFFICIENT
        )
        coefs = self._elemental.get_attrs(
            enums.DoubleAttr2.QUADRATIC_CONSTRAINT_LINEAR_COEFFICIENT, keys
        )
        for i in range(len(keys)):
            yield (
                quadratic_constraints.QuadraticConstraint(
                    self._elemental, int(keys[i, 0])
                ),
                variables_mod.Variable(self._elemental, int(keys[i, 1])),
                float(coefs[i]),
            )

    def quadratic_constraint_quadratic_nonzeros(
        self,
    ) -> Iterator[
        Tuple[
            quadratic_constraints.QuadraticConstraint,
            variables_mod.Variable,
            variables_mod.Variable,
            float,
        ]
    ]:
        """Yields the quadratic coefficients for all quadratic constraints in the model."""
        keys = self._elemental.get_attr_non_defaults(
            enums.SymmetricDoubleAttr3.QUADRATIC_CONSTRAINT_QUADRATIC_COEFFICIENT
        )
        coefs = self._elemental.get_attrs(
            enums.SymmetricDoubleAttr3.QUADRATIC_CONSTRAINT_QUADRATIC_COEFFICIENT,
            keys,
        )
        for i in range(len(keys)):
            yield (
                quadratic_constraints.QuadraticConstraint(
                    self._elemental, int(keys[i, 0])
                ),
                variables_mod.Variable(self._elemental, int(keys[i, 1])),
                variables_mod.Variable(self._elemental, int(keys[i, 2])),
                float(coefs[i]),
            )

    ##############################################################################
    # Indicator Constraints
    ##############################################################################

    def add_indicator_constraint(
        self,
        *,
        indicator: Optional[variables_mod.Variable] = None,
        activate_on_zero: bool = False,
        implied_constraint: Optional[
            Union[bool, variables_mod.BoundedLinearTypes]
        ] = None,
        implied_lb: Optional[float] = None,
        implied_ub: Optional[float] = None,
        implied_expr: Optional[variables_mod.LinearTypes] = None,
        name: str = "",
    ) -> indicator_constraints.IndicatorConstraint:
        """Adds an indicator constraint to the model.

        If indicator is None or the variable equal to indicator is deleted from
        the model, the model will be considered invalid at solve time (unless this
        constraint is also deleted before solving). Likewise, the variable indicator
        must be binary at solve time for the model to be valid.

        If implied_constraint is set, you may not set implied_lb, implied_ub, or
        implied_expr.

        Args:
          indicator: The variable whose value determines if implied_constraint must
            be enforced.
          activate_on_zero: If true, implied_constraint must hold when indicator is
            zero, otherwise, the implied_constraint must hold when indicator is one.
          implied_constraint: A linear constraint to conditionally enforce, if set.
            If None, that information is instead passed via implied_lb, implied_ub,
            and implied_expr.
          implied_lb: The lower bound of the condtionally enforced linear constraint
            (or -inf if None), used only when implied_constraint is None.
          implied_ub: The upper bound of the condtionally enforced linear constraint
            (or +inf if None), used only when implied_constraint is None.
          implied_expr: The linear part of the condtionally enforced linear
            constraint (or 0 if None), used only when implied_constraint is None. If
            expr has a nonzero offset, it is subtracted from lb and ub.
          name: For debugging purposes only, but nonempty names must be distinct.

        Returns:
          A reference to the new indicator constraint.
        """
        ind_con_id = self._elemental.add_element(
            enums.ElementType.INDICATOR_CONSTRAINT, name
        )
        if indicator is not None:
            self._elemental.set_attr(
                enums.VariableAttr1.INDICATOR_CONSTRAINT_INDICATOR,
                (ind_con_id,),
                indicator.id,
            )
        self._elemental.set_attr(
            enums.BoolAttr1.INDICATOR_CONSTRAINT_ACTIVATE_ON_ZERO,
            (ind_con_id,),
            activate_on_zero,
        )
        implied_inequality = normalized_inequality.as_normalized_linear_inequality(
            implied_constraint, lb=implied_lb, ub=implied_ub, expr=implied_expr
        )
        self._elemental.set_attr(
            enums.DoubleAttr1.INDICATOR_CONSTRAINT_LOWER_BOUND,
            (ind_con_id,),
            implied_inequality.lb,
        )
        self._elemental.set_attr(
            enums.DoubleAttr1.INDICATOR_CONSTRAINT_UPPER_BOUND,
            (ind_con_id,),
            implied_inequality.ub,
        )
        for var, coef in implied_inequality.coefficients.items():
            self._elemental.set_attr(
                enums.DoubleAttr2.INDICATOR_CONSTRAINT_LINEAR_COEFFICIENT,
                (ind_con_id, var.id),
                coef,
            )

        return indicator_constraints.IndicatorConstraint(self._elemental, ind_con_id)

    def has_indicator_constraint(self, con_id: int) -> bool:
        """Returns true if an indicator constraint with this id is in the model."""
        return self._elemental.element_exists(
            enums.ElementType.INDICATOR_CONSTRAINT, con_id
        )

    def get_num_indicator_constraints(self) -> int:
        """Returns the number of indicator constraints in the model."""
        return self._elemental.get_num_elements(enums.ElementType.INDICATOR_CONSTRAINT)

    def get_next_indicator_constraint_id(self) -> int:
        """Returns the id of the next indicator constraint created in the model."""
        return self._elemental.get_next_element_id(
            enums.ElementType.INDICATOR_CONSTRAINT
        )

    def ensure_next_indicator_constraint_id_at_least(self, con_id: int) -> None:
        """If the next indicator constraint id would be less than `con_id`, sets it to `con_id`."""
        self._elemental.ensure_next_element_id_at_least(
            enums.ElementType.INDICATOR_CONSTRAINT, con_id
        )

    def get_indicator_constraint(
        self, con_id: int, *, validate: bool = True
    ) -> indicator_constraints.IndicatorConstraint:
        """Returns the IndicatorConstraint for the id con_id."""
        if validate and not self._elemental.element_exists(
            enums.ElementType.INDICATOR_CONSTRAINT, con_id
        ):
            raise KeyError(f"Indicator constraint does not exist with id {con_id}.")
        return indicator_constraints.IndicatorConstraint(self._elemental, con_id)

    def delete_indicator_constraint(
        self, ind_con: indicator_constraints.IndicatorConstraint
    ) -> None:
        self.check_compatible(ind_con)
        if not self._elemental.delete_element(
            enums.ElementType.INDICATOR_CONSTRAINT, ind_con.id
        ):
            raise ValueError(
                f"Indicator constraint with id {ind_con.id} was not in the model."
            )

    def get_indicator_constraints(
        self,
    ) -> Iterator[indicator_constraints.IndicatorConstraint]:
        """Yields the indicator constraints in the order of creation."""
        ind_con_ids = self._elemental.get_elements(
            enums.ElementType.INDICATOR_CONSTRAINT
        )
        ind_con_ids.sort()
        for ind_con_id in ind_con_ids:
            yield indicator_constraints.IndicatorConstraint(
                self._elemental, int(ind_con_id)
            )

    ##############################################################################
    # Proto import/export
    ##############################################################################

    def export_model(self) -> model_pb2.ModelProto:
        """Returns a protocol buffer equivalent to this model."""
        return self._elemental.export_model(remove_names=False)

    def add_update_tracker(self) -> UpdateTracker:
        """Creates an UpdateTracker registered on this model to view changes."""
        return UpdateTracker(self._elemental.add_diff(), self._elemental)

    def remove_update_tracker(self, tracker: UpdateTracker):
        """Stops tracker from getting updates on changes to this model.

        An error will be raised if tracker was not created by this Model or if
        tracker has been previously removed.

        Using (via checkpoint or update) an UpdateTracker after it has been removed
        will result in an error.

        Args:
          tracker: The UpdateTracker to unregister.

        Raises:
          KeyError: The tracker was created by another model or was already removed.
        """
        self._elemental.delete_diff(tracker.diff_id)

    def check_compatible(self, e: from_model.FromModel) -> None:
        """Raises a ValueError if the model of var_or_constraint is not self."""
        if e.elemental is not self._elemental:
            raise ValueError(
                f"Expected element from model named: '{self._elemental.model_name}',"
                f" but observed element {e} from model named:"
                f" '{e.elemental.model_name}'."
            )
