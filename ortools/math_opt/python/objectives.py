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

"""An Objective for a MathOpt optimization model."""

import abc
from typing import Any, Iterator

from ortools.math_opt.elemental.python import enums
from ortools.math_opt.python import from_model
from ortools.math_opt.python import variables
from ortools.math_opt.python.elemental import elemental


class Objective(from_model.FromModel, metaclass=abc.ABCMeta):
    """The objective for an optimization model.

    An objective is either of the form:
      min o + sum_{i in I} c_i * x_i + sum_{i, j in I, i <= j} q_i,j * x_i * x_j
    or
      max o + sum_{i in I} c_i * x_i + sum_{(i, j) in Q} q_i,j * x_i * x_j
    where x_i are the decision variables of the problem and where all pairs (i, j)
    in Q satisfy i <= j. The values of o, c_i and q_i,j should be finite and not
    NaN.

    The objective can be configured as follows:
      * offset: a float property, o above. Should be finite and not NaN.
      * is_maximize: a bool property, if the objective is to maximize or minimize.
      * set_linear_coefficient and get_linear_coefficient control the c_i * x_i
        terms. The variables must be from the same model as this objective, and
        the c_i must be finite and not NaN. The coefficient for any variable not
        set is 0.0, and setting a coefficient to 0.0 removes it from I above.
      * set_quadratic_coefficient and get_quadratic_coefficient control the
        q_i,j * x_i * x_j terms. The variables must be from the same model as this
        objective, and the q_i,j must be finite and not NaN. The coefficient for
        any pair of variables not set is 0.0, and setting a coefficient to 0.0
        removes the associated (i,j) from Q above.

    Do not create an Objective directly, use Model.objective to access the
    objective instead (or Model.add_auxiliary_objective()). Two Objective objects
    can represent the same objective (for the same model). They will have the same
    underlying Objective.elemental for storing the data. The Objective class is
    simply a reference to an Elemental.

    The objective is linear if only linear coefficients are set. This can be
    useful to avoid solve-time errors with solvers that do not accept quadratic
    objectives. To facilitate this linear objective guarantee we provide three
    functions to add to the objective:
      * add(), which accepts linear or quadratic expressions,
      * add_quadratic(), which also accepts linear or quadratic expressions and
        can be used to signal a quadratic objective is possible, and
      * add_linear(), which only accepts linear expressions and can be used to
        guarantee the objective remains linear.

    For quadratic terms, the order that variables are provided does not matter,
    we always canonicalize to first_var <= second_var. So if you set (x1, x2) to 7
    then:
      * getting (x2, x1) returns 7
      * setting (x2, x1) to 10 overwrites the value of 7.
    Likewise, when we return nonzero quadratic coefficients, we always use the
    form first_var <= second_var.

    Most problems have only a single objective, but hierarchical objectives are
    supported (see Model.add_auxiliary_objective()). Note that quadratic Auxiliary
    objectives are not supported.
    """

    __slots__ = ("_elemental",)

    def __init__(self, elem: elemental.Elemental) -> None:
        """Do not invoke directly, prefer Model.objective."""
        self._elemental: elemental.Elemental = elem

    @property
    def elemental(self) -> elemental.Elemental:
        """The underlying data structure for the model, for internal use only."""
        return self._elemental

    @property
    @abc.abstractmethod
    def name(self) -> str:
        """The immutable name of this objective, for display only."""

    @property
    @abc.abstractmethod
    def is_maximize(self) -> bool:
        """If true, the direction is maximization, otherwise minimization."""

    @is_maximize.setter
    @abc.abstractmethod
    def is_maximize(self, is_maximize: bool) -> None: ...

    @property
    @abc.abstractmethod
    def offset(self) -> float:
        """A constant added to the objective."""

    @offset.setter
    @abc.abstractmethod
    def offset(self, value: float) -> None: ...

    @property
    @abc.abstractmethod
    def priority(self) -> int:
        """For hierarchical problems, determines the order to apply objectives.

        The objectives are applied from lowest priority to highest.

        The default priority for the primary objective is zero, and auxiliary
        objectives must specific a priority at creation time.

        Priority has no effect for problems with only one objective.
        """

    @priority.setter
    @abc.abstractmethod
    def priority(self, value: int) -> None: ...

    @abc.abstractmethod
    def set_linear_coefficient(self, var: variables.Variable, coef: float) -> None:
        """Sets the coefficient of `var` to `coef` in the objective."""

    @abc.abstractmethod
    def get_linear_coefficient(self, var: variables.Variable) -> float:
        """Returns the coefficinet of `var` (or zero if unset)."""

    @abc.abstractmethod
    def linear_terms(self) -> Iterator[variables.LinearTerm]:
        """Yields variable coefficient pairs for variables with nonzero objective coefficient in undefined order."""

    @abc.abstractmethod
    def set_quadratic_coefficient(
        self,
        first_variable: variables.Variable,
        second_variable: variables.Variable,
        coef: float,
    ) -> None:
        """Sets the coefficient for product of variables (see class description)."""

    @abc.abstractmethod
    def get_quadratic_coefficient(
        self,
        first_variable: variables.Variable,
        second_variable: variables.Variable,
    ) -> float:
        """Gets the coefficient for product of variables (see class description)."""

    @abc.abstractmethod
    def quadratic_terms(self) -> Iterator[variables.QuadraticTerm]:
        """Yields quadratic terms with nonzero objective coefficient in undefined order."""

    @abc.abstractmethod
    def clear(self) -> None:
        """Clears objective coefficients and offset. Does not change direction."""

    def as_linear_expression(self) -> variables.LinearExpression:
        """Returns an equivalent LinearExpression, or errors if quadratic."""
        if any(self.quadratic_terms()):
            raise TypeError("Cannot get a quadratic objective as a linear expression")
        return variables.as_flat_linear_expression(
            self.offset + variables.LinearSum(self.linear_terms())
        )

    def as_quadratic_expression(self) -> variables.QuadraticExpression:
        """Returns an equivalent QuadraticExpression to this objetive."""
        return variables.as_flat_quadratic_expression(
            self.offset
            + variables.LinearSum(self.linear_terms())
            + variables.QuadraticSum(self.quadratic_terms())
        )

    def add(self, objective: variables.QuadraticTypes) -> None:
        """Adds the provided expression `objective` to the objective function.

        For a compile time guarantee that the objective remains linear, use
        add_linear() instead.

        Args:
          objective: the expression to add to the objective function.
        """
        if isinstance(objective, (variables.LinearBase, int, float)):
            self.add_linear(objective)
        elif isinstance(objective, variables.QuadraticBase):
            self.add_quadratic(objective)
        else:
            raise TypeError(
                "unsupported type in objective argument for "
                f"Objective.add(): {type(objective).__name__!r}"
            )

    def add_linear(self, objective: variables.LinearTypes) -> None:
        """Adds the provided linear expression `objective` to the objective function."""
        if not isinstance(objective, (variables.LinearBase, int, float)):
            raise TypeError(
                "unsupported type in objective argument for "
                f"Objective.add_linear(): {type(objective).__name__!r}"
            )
        objective_expr = variables.as_flat_linear_expression(objective)
        self.offset += objective_expr.offset
        for var, coefficient in objective_expr.terms.items():
            self.set_linear_coefficient(
                var, self.get_linear_coefficient(var) + coefficient
            )

    def add_quadratic(self, objective: variables.QuadraticTypes) -> None:
        """Adds the provided quadratic expression `objective` to the objective function."""
        if not isinstance(
            objective, (variables.QuadraticBase, variables.LinearBase, int, float)
        ):
            raise TypeError(
                "unsupported type in objective argument for "
                f"Objective.add(): {type(objective).__name__!r}"
            )
        objective_expr = variables.as_flat_quadratic_expression(objective)
        self.offset += objective_expr.offset
        for var, coefficient in objective_expr.linear_terms.items():
            self.set_linear_coefficient(
                var, self.get_linear_coefficient(var) + coefficient
            )
        for key, coefficient in objective_expr.quadratic_terms.items():
            self.set_quadratic_coefficient(
                key.first_var,
                key.second_var,
                self.get_quadratic_coefficient(key.first_var, key.second_var)
                + coefficient,
            )

    def set_to_linear_expression(self, linear_expr: variables.LinearTypes) -> None:
        """Sets the objective to optimize to `linear_expr`."""
        if not isinstance(linear_expr, (variables.LinearBase, int, float)):
            raise TypeError(
                "unsupported type in objective argument for "
                f"set_to_linear_expression: {type(linear_expr).__name__!r}"
            )
        self.clear()
        objective_expr = variables.as_flat_linear_expression(linear_expr)
        self.offset = objective_expr.offset
        for var, coefficient in objective_expr.terms.items():
            self.set_linear_coefficient(var, coefficient)

    def set_to_quadratic_expression(
        self, quadratic_expr: variables.QuadraticTypes
    ) -> None:
        """Sets the objective to optimize the `quadratic_expr`."""
        if not isinstance(
            quadratic_expr,
            (variables.QuadraticBase, variables.LinearBase, int, float),
        ):
            raise TypeError(
                "unsupported type in objective argument for "
                f"set_to_quadratic_expression: {type(quadratic_expr).__name__!r}"
            )
        self.clear()
        objective_expr = variables.as_flat_quadratic_expression(quadratic_expr)
        self.offset = objective_expr.offset
        for var, coefficient in objective_expr.linear_terms.items():
            self.set_linear_coefficient(var, coefficient)
        for quad_key, coefficient in objective_expr.quadratic_terms.items():
            self.set_quadratic_coefficient(
                quad_key.first_var, quad_key.second_var, coefficient
            )

    def set_to_expression(self, expr: variables.QuadraticTypes) -> None:
        """Sets the objective to optimize the `expr`."""
        if isinstance(expr, (variables.LinearBase, int, float)):
            self.set_to_linear_expression(expr)
        elif isinstance(expr, variables.QuadraticBase):
            self.set_to_quadratic_expression(expr)
        else:
            raise TypeError(
                "unsupported type in objective argument for "
                f"set_to_expression: {type(expr).__name__!r}"
            )


class PrimaryObjective(Objective):
    """The main objective, but users should program against Objective directly."""

    __slots__ = ()

    @property
    def name(self) -> str:
        return self._elemental.primary_objective_name

    @property
    def is_maximize(self) -> bool:
        return self._elemental.get_attr(enums.BoolAttr0.MAXIMIZE, ())

    @is_maximize.setter
    def is_maximize(self, is_maximize: bool) -> None:
        self._elemental.set_attr(enums.BoolAttr0.MAXIMIZE, (), is_maximize)

    @property
    def offset(self) -> float:
        return self._elemental.get_attr(enums.DoubleAttr0.OBJECTIVE_OFFSET, ())

    @offset.setter
    def offset(self, value: float) -> None:
        self._elemental.set_attr(enums.DoubleAttr0.OBJECTIVE_OFFSET, (), value)

    @property
    def priority(self) -> int:
        return self._elemental.get_attr(enums.IntAttr0.OBJECTIVE_PRIORITY, ())

    @priority.setter
    def priority(self, value: int) -> None:
        self._elemental.set_attr(enums.IntAttr0.OBJECTIVE_PRIORITY, (), value)

    def set_linear_coefficient(self, var: variables.Variable, coef: float) -> None:
        from_model.model_is_same(self, var)
        self._elemental.set_attr(
            enums.DoubleAttr1.OBJECTIVE_LINEAR_COEFFICIENT, (var.id,), coef
        )

    def get_linear_coefficient(self, var: variables.Variable) -> float:
        from_model.model_is_same(self, var)
        return self._elemental.get_attr(
            enums.DoubleAttr1.OBJECTIVE_LINEAR_COEFFICIENT, (var.id,)
        )

    def linear_terms(self) -> Iterator[variables.LinearTerm]:
        keys = self._elemental.get_attr_non_defaults(
            enums.DoubleAttr1.OBJECTIVE_LINEAR_COEFFICIENT
        )
        var_index = 0
        coefs = self._elemental.get_attrs(
            enums.DoubleAttr1.OBJECTIVE_LINEAR_COEFFICIENT, keys
        )
        for i in range(len(keys)):
            yield variables.LinearTerm(
                variable=variables.Variable(self._elemental, int(keys[i, var_index])),
                coefficient=float(coefs[i]),
            )

    def set_quadratic_coefficient(
        self,
        first_variable: variables.Variable,
        second_variable: variables.Variable,
        coef: float,
    ) -> None:
        from_model.model_is_same(self, first_variable)
        from_model.model_is_same(self, second_variable)
        self._elemental.set_attr(
            enums.SymmetricDoubleAttr2.OBJECTIVE_QUADRATIC_COEFFICIENT,
            (first_variable.id, second_variable.id),
            coef,
        )

    def get_quadratic_coefficient(
        self,
        first_variable: variables.Variable,
        second_variable: variables.Variable,
    ) -> float:
        from_model.model_is_same(self, first_variable)
        from_model.model_is_same(self, second_variable)
        return self._elemental.get_attr(
            enums.SymmetricDoubleAttr2.OBJECTIVE_QUADRATIC_COEFFICIENT,
            (first_variable.id, second_variable.id),
        )

    def quadratic_terms(self) -> Iterator[variables.QuadraticTerm]:
        keys = self._elemental.get_attr_non_defaults(
            enums.SymmetricDoubleAttr2.OBJECTIVE_QUADRATIC_COEFFICIENT
        )
        coefs = self._elemental.get_attrs(
            enums.SymmetricDoubleAttr2.OBJECTIVE_QUADRATIC_COEFFICIENT, keys
        )
        for i in range(len(keys)):
            yield variables.QuadraticTerm(
                variables.QuadraticTermKey(
                    variables.Variable(self._elemental, int(keys[i, 0])),
                    variables.Variable(self._elemental, int(keys[i, 1])),
                ),
                coefficient=float(coefs[i]),
            )

    def clear(self) -> None:
        self._elemental.clear_attr(enums.DoubleAttr0.OBJECTIVE_OFFSET)
        self._elemental.clear_attr(enums.DoubleAttr1.OBJECTIVE_LINEAR_COEFFICIENT)
        self._elemental.clear_attr(
            enums.SymmetricDoubleAttr2.OBJECTIVE_QUADRATIC_COEFFICIENT
        )

    def __eq__(self, other: Any) -> bool:
        if isinstance(other, PrimaryObjective):
            return self._elemental is other._elemental
        return False

    def __hash__(self) -> int:
        return hash(self._elemental)


class AuxiliaryObjective(Objective):
    """An additional objective that can be optimized after objectives."""

    __slots__ = ("_id",)

    def __init__(self, elem: elemental.Elemental, obj_id: int) -> None:
        """Internal only, prefer Model functions (add_auxiliary_objective() and get_auxiliary_objective())."""
        super().__init__(elem)
        if not isinstance(obj_id, int):
            raise TypeError(
                f"obj_id type should be int, was: {type(obj_id).__name__!r}"
            )
        self._id: int = obj_id

    @property
    def name(self) -> str:
        return self._elemental.get_element_name(
            enums.ElementType.AUXILIARY_OBJECTIVE, self._id
        )

    @property
    def id(self) -> int:
        """Returns the id of this objective."""
        return self._id

    @property
    def is_maximize(self) -> bool:
        return self._elemental.get_attr(
            enums.BoolAttr1.AUXILIARY_OBJECTIVE_MAXIMIZE, (self._id,)
        )

    @is_maximize.setter
    def is_maximize(self, is_maximize: bool) -> None:
        self._elemental.set_attr(
            enums.BoolAttr1.AUXILIARY_OBJECTIVE_MAXIMIZE,
            (self._id,),
            is_maximize,
        )

    @property
    def offset(self) -> float:
        return self._elemental.get_attr(
            enums.DoubleAttr1.AUXILIARY_OBJECTIVE_OFFSET, (self._id,)
        )

    @offset.setter
    def offset(self, value: float) -> None:
        self._elemental.set_attr(
            enums.DoubleAttr1.AUXILIARY_OBJECTIVE_OFFSET,
            (self._id,),
            value,
        )

    @property
    def priority(self) -> int:
        return self._elemental.get_attr(
            enums.IntAttr1.AUXILIARY_OBJECTIVE_PRIORITY, (self._id,)
        )

    @priority.setter
    def priority(self, value: int) -> None:
        self._elemental.set_attr(
            enums.IntAttr1.AUXILIARY_OBJECTIVE_PRIORITY,
            (self._id,),
            value,
        )

    def set_linear_coefficient(self, var: variables.Variable, coef: float) -> None:
        from_model.model_is_same(self, var)
        self._elemental.set_attr(
            enums.DoubleAttr2.AUXILIARY_OBJECTIVE_LINEAR_COEFFICIENT,
            (self._id, var.id),
            coef,
        )

    def get_linear_coefficient(self, var: variables.Variable) -> float:
        from_model.model_is_same(self, var)
        return self._elemental.get_attr(
            enums.DoubleAttr2.AUXILIARY_OBJECTIVE_LINEAR_COEFFICIENT,
            (
                self._id,
                var.id,
            ),
        )

    def linear_terms(self) -> Iterator[variables.LinearTerm]:
        keys = self._elemental.slice_attr(
            enums.DoubleAttr2.AUXILIARY_OBJECTIVE_LINEAR_COEFFICIENT,
            0,
            self._id,
        )
        var_index = 1
        coefs = self._elemental.get_attrs(
            enums.DoubleAttr2.AUXILIARY_OBJECTIVE_LINEAR_COEFFICIENT, keys
        )
        for i in range(len(keys)):
            yield variables.LinearTerm(
                variable=variables.Variable(self._elemental, int(keys[i, var_index])),
                coefficient=float(coefs[i]),
            )

    def set_quadratic_coefficient(
        self,
        first_variable: variables.Variable,
        second_variable: variables.Variable,
        coef: float,
    ) -> None:
        raise ValueError("Quadratic auxiliary objectives are not supported.")

    def get_quadratic_coefficient(
        self,
        first_variable: variables.Variable,
        second_variable: variables.Variable,
    ) -> float:
        from_model.model_is_same(self, first_variable)
        from_model.model_is_same(self, second_variable)
        if not self._elemental.element_exists(
            enums.ElementType.VARIABLE, first_variable.id
        ):
            raise ValueError(f"Variable {first_variable} does not exist")
        if not self._elemental.element_exists(
            enums.ElementType.VARIABLE, second_variable.id
        ):
            raise ValueError(f"Variable {second_variable} does not exist")
        return 0.0

    def quadratic_terms(self) -> Iterator[variables.QuadraticTerm]:
        return iter(())

    def clear(self) -> None:
        """Clears objective coefficients and offset. Does not change direction."""
        self._elemental.clear_attr(enums.DoubleAttr1.AUXILIARY_OBJECTIVE_OFFSET)
        self._elemental.clear_attr(
            enums.DoubleAttr2.AUXILIARY_OBJECTIVE_LINEAR_COEFFICIENT
        )

    def __eq__(self, other: Any) -> bool:
        if isinstance(other, AuxiliaryObjective):
            return self._elemental is other._elemental and self._id == other._id
        return False

    def __hash__(self) -> int:
        return hash((self._elemental, self._id))
