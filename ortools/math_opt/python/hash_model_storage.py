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

"""A minimal pure python implementation of model_storage.ModelStorage."""

from typing import Dict, Iterable, Iterator, Optional, Set, Tuple
import weakref

from ortools.math_opt import model_pb2
from ortools.math_opt import model_update_pb2
from ortools.math_opt import sparse_containers_pb2
from ortools.math_opt.python import model_storage

_QuadraticKey = model_storage.QuadraticTermIdKey


class _UpdateTracker(model_storage.StorageUpdateTracker):
    """Tracks model updates for HashModelStorage."""

    def __init__(self, mod: "HashModelStorage"):
        self.retired: bool = False
        self.model: "HashModelStorage" = mod
        # Changes for variables with id < variables_checkpoint are explicitly
        # tracked.
        self.variables_checkpoint: int = self.model._next_var_id
        # Changes for linear constraints with id < linear_constraints_checkpoint
        # are explicitly tracked.
        self.linear_constraints_checkpoint: int = self.model._next_lin_con_id

        self.objective_direction: bool = False
        self.objective_offset: bool = False

        self.variable_deletes: Set[int] = set()
        self.variable_lbs: Set[int] = set()
        self.variable_ubs: Set[int] = set()
        self.variable_integers: Set[int] = set()

        self.linear_objective_coefficients: Set[int] = set()
        self.quadratic_objective_coefficients: Set[_QuadraticKey] = set()

        self.linear_constraint_deletes: Set[int] = set()
        self.linear_constraint_lbs: Set[int] = set()
        self.linear_constraint_ubs: Set[int] = set()

        self.linear_constraint_matrix: Set[Tuple[int, int]] = set()

    def export_update(self) -> Optional[model_update_pb2.ModelUpdateProto]:
        if self.retired:
            raise model_storage.UsedUpdateTrackerAfterRemovalError()
        if (
            self.variables_checkpoint == self.model.next_variable_id()
            and (
                self.linear_constraints_checkpoint
                == self.model.next_linear_constraint_id()
            )
            and not self.objective_direction
            and not self.objective_offset
            and not self.variable_deletes
            and not self.variable_lbs
            and not self.variable_ubs
            and not self.variable_integers
            and not self.linear_objective_coefficients
            and not self.quadratic_objective_coefficients
            and not self.linear_constraint_deletes
            and not self.linear_constraint_lbs
            and not self.linear_constraint_ubs
            and not self.linear_constraint_matrix
        ):
            return None
        result = model_update_pb2.ModelUpdateProto()
        result.deleted_variable_ids[:] = sorted(self.variable_deletes)
        result.deleted_linear_constraint_ids[:] = sorted(self.linear_constraint_deletes)
        # Variable updates
        _set_sparse_double_vector(
            sorted((vid, self.model.get_variable_lb(vid)) for vid in self.variable_lbs),
            result.variable_updates.lower_bounds,
        )
        _set_sparse_double_vector(
            sorted((vid, self.model.get_variable_ub(vid)) for vid in self.variable_ubs),
            result.variable_updates.upper_bounds,
        )
        _set_sparse_bool_vector(
            sorted(
                (vid, self.model.get_variable_is_integer(vid))
                for vid in self.variable_integers
            ),
            result.variable_updates.integers,
        )
        # Linear constraint updates
        _set_sparse_double_vector(
            sorted(
                (cid, self.model.get_linear_constraint_lb(cid))
                for cid in self.linear_constraint_lbs
            ),
            result.linear_constraint_updates.lower_bounds,
        )
        _set_sparse_double_vector(
            sorted(
                (cid, self.model.get_linear_constraint_ub(cid))
                for cid in self.linear_constraint_ubs
            ),
            result.linear_constraint_updates.upper_bounds,
        )
        # New variables and constraints
        new_vars = []
        for vid in range(self.variables_checkpoint, self.model.next_variable_id()):
            var = self.model.variables.get(vid)
            if var is not None:
                new_vars.append((vid, var))
        _variables_to_proto(new_vars, result.new_variables)
        new_lin_cons = []
        for lin_con_id in range(
            self.linear_constraints_checkpoint,
            self.model.next_linear_constraint_id(),
        ):
            lin_con = self.model.linear_constraints.get(lin_con_id)
            if lin_con is not None:
                new_lin_cons.append((lin_con_id, lin_con))
        _linear_constraints_to_proto(new_lin_cons, result.new_linear_constraints)
        # Objective update
        if self.objective_direction:
            result.objective_updates.direction_update = self.model.get_is_maximize()
        if self.objective_offset:
            result.objective_updates.offset_update = self.model.get_objective_offset()
        _set_sparse_double_vector(
            sorted(
                (var, self.model.get_linear_objective_coefficient(var))
                for var in self.linear_objective_coefficients
            ),
            result.objective_updates.linear_coefficients,
        )
        for new_var in range(self.variables_checkpoint, self.model.next_variable_id()):
            # NOTE: the value will be 0.0 if either the coefficient is not set or the
            # variable has been deleted. Calling
            # model.get_linear_objective_coefficient() throws an exception if the
            # variable has been deleted.
            obj_coef = self.model.linear_objective_coefficient.get(new_var, 0.0)
            if obj_coef:
                result.objective_updates.linear_coefficients.ids.append(new_var)
                result.objective_updates.linear_coefficients.values.append(obj_coef)

        quadratic_objective_updates = [
            (
                key.id1,
                key.id2,
                self.model.get_quadratic_objective_coefficient(key.id1, key.id2),
            )
            for key in self.quadratic_objective_coefficients
        ]
        for new_var in range(self.variables_checkpoint, self.model.next_variable_id()):
            if self.model.variable_exists(new_var):
                for other_var in self.model.get_quadratic_objective_adjacent_variables(
                    new_var
                ):
                    key = _QuadraticKey(new_var, other_var)
                    if new_var >= other_var:
                        key = _QuadraticKey(new_var, other_var)
                        quadratic_objective_updates.append(
                            (
                                key.id1,
                                key.id2,
                                self.model.get_quadratic_objective_coefficient(
                                    key.id1, key.id2
                                ),
                            )
                        )
        quadratic_objective_updates.sort()
        if quadratic_objective_updates:
            first_var_ids, second_var_ids, coefficients = zip(
                *quadratic_objective_updates
            )
            result.objective_updates.quadratic_coefficients.row_ids[:] = first_var_ids
            result.objective_updates.quadratic_coefficients.column_ids[:] = (
                second_var_ids
            )
            result.objective_updates.quadratic_coefficients.coefficients[:] = (
                coefficients
            )
        # Linear constraint matrix updates
        matrix_updates = [
            (l, v, self.model.get_linear_constraint_coefficient(l, v))
            for (l, v) in self.linear_constraint_matrix
        ]
        for new_var in range(self.variables_checkpoint, self.model.next_variable_id()):
            if self.model.variable_exists(new_var):
                for lin_con in self.model.get_linear_constraints_with_variable(new_var):
                    matrix_updates.append(
                        (
                            lin_con,
                            new_var,
                            self.model.get_linear_constraint_coefficient(
                                lin_con, new_var
                            ),
                        )
                    )
        for new_lin_con in range(
            self.linear_constraints_checkpoint,
            self.model.next_linear_constraint_id(),
        ):
            if self.model.linear_constraint_exists(new_lin_con):
                for var in self.model.get_variables_for_linear_constraint(new_lin_con):
                    # We have already gotten the new variables above. Note that we do at
                    # most twice as much work as we should from this.
                    if var < self.variables_checkpoint:
                        matrix_updates.append(
                            (
                                new_lin_con,
                                var,
                                self.model.get_linear_constraint_coefficient(
                                    new_lin_con, var
                                ),
                            )
                        )
        matrix_updates.sort()
        if matrix_updates:
            lin_cons, variables, coefs = zip(*matrix_updates)
            result.linear_constraint_matrix_updates.row_ids[:] = lin_cons
            result.linear_constraint_matrix_updates.column_ids[:] = variables
            result.linear_constraint_matrix_updates.coefficients[:] = coefs
        return result

    def advance_checkpoint(self) -> None:
        if self.retired:
            raise model_storage.UsedUpdateTrackerAfterRemovalError()
        self.objective_direction = False
        self.objective_offset = False
        self.variable_deletes = set()
        self.variable_lbs = set()
        self.variable_ubs = set()
        self.variable_integers = set()
        self.linear_objective_coefficients = set()
        self.linear_constraint_deletes = set()
        self.linear_constraint_lbs = set()
        self.linear_constraint_ubs = set()
        self.linear_constraint_matrix = set()

        self.variables_checkpoint = self.model.next_variable_id()
        self.linear_constraints_checkpoint = self.model.next_linear_constraint_id()


class _VariableStorage:
    """Data specific to each decision variable in the optimization problem."""

    def __init__(self, lb: float, ub: float, is_integer: bool, name: str) -> None:
        self.lower_bound: float = lb
        self.upper_bound: float = ub
        self.is_integer: bool = is_integer
        self.name: str = name
        self.linear_constraint_nonzeros: Set[int] = set()


class _LinearConstraintStorage:
    """Data specific to each linear constraint in the optimization problem."""

    def __init__(self, lb: float, ub: float, name: str) -> None:
        self.lower_bound: float = lb
        self.upper_bound: float = ub
        self.name: str = name
        self.variable_nonzeros: Set[int] = set()


class _QuadraticTermStorage:
    """Data describing quadratic terms with non-zero coefficients."""

    def __init__(self) -> None:
        self._coefficients: Dict[_QuadraticKey, float] = {}
        # For a variable i that does not appear in a quadratic objective term with
        # a non-zero coefficient, we may have self._adjacent_variable[i] being an
        # empty set or i not appearing in self._adjacent_variable.keys() (e.g.
        # depeding on whether the variable previously appeared in a quadratic term).
        self._adjacent_variables: Dict[int, Set[int]] = {}

    def __bool__(self) -> bool:
        """Returns true if and only if there are any quadratic terms with non-zero coefficients."""
        return bool(self._coefficients)

    def get_adjacent_variables(self, variable_id: int) -> Iterator[int]:
        """Yields the variables multiplying a variable in the stored quadratic terms.

        If variable_id is not in the model the function yields the empty set.

        Args:
          variable_id: Function yields the variables multiplying variable_id in the
            stored quadratic terms.

        Yields:
          The variables multiplying variable_id in the stored quadratic terms.
        """
        yield from self._adjacent_variables.get(variable_id, ())

    def keys(self) -> Iterator[_QuadraticKey]:
        """Yields the variable-pair keys associated to the stored quadratic terms."""
        yield from self._coefficients.keys()

    def coefficients(self) -> Iterator[model_storage.QuadraticEntry]:
        """Yields the stored quadratic terms as QuadraticEntry."""
        for key, coef in self._coefficients.items():
            yield model_storage.QuadraticEntry(id_key=key, coefficient=coef)

    def delete_variable(self, variable_id: int) -> None:
        """Updates the data structure to consider variable_id as deleted."""
        if variable_id not in self._adjacent_variables.keys():
            return
        for adjacent_variable_id in self._adjacent_variables[variable_id]:
            if variable_id != adjacent_variable_id:
                self._adjacent_variables[adjacent_variable_id].remove(variable_id)
            del self._coefficients[_QuadraticKey(variable_id, adjacent_variable_id)]
        self._adjacent_variables[variable_id].clear()

    def clear(self) -> None:
        """Clears the data structure."""
        self._coefficients.clear()
        self._adjacent_variables.clear()

    def set_coefficient(
        self, first_variable_id: int, second_variable_id: int, value: float
    ) -> bool:
        """Sets the coefficient for the quadratic term associated to the product between two variables.

        The ordering of the input variables does not matter.

        Args:
          first_variable_id: The first variable in the product.
          second_variable_id: The second variable in the product.
          value: The value of the coefficient.

        Returns:
          True if the coefficient is updated, False otherwise.
        """
        key = _QuadraticKey(first_variable_id, second_variable_id)
        if value == self._coefficients.get(key, 0.0):
            return False
        if value == 0.0:
            # Assuming self._coefficients/_adjacent_variables are filled according
            # to get_coefficient(key) != 0.0.
            del self._coefficients[key]
            self._adjacent_variables[first_variable_id].remove(second_variable_id)
            if first_variable_id != second_variable_id:
                self._adjacent_variables[second_variable_id].remove(first_variable_id)
        else:
            if first_variable_id not in self._adjacent_variables.keys():
                self._adjacent_variables[first_variable_id] = set()
            if second_variable_id not in self._adjacent_variables.keys():
                self._adjacent_variables[second_variable_id] = set()
            self._coefficients[key] = value
            self._adjacent_variables[first_variable_id].add(second_variable_id)
            self._adjacent_variables[second_variable_id].add(first_variable_id)
        return True

    def get_coefficient(self, first_variable_id: int, second_variable_id: int) -> float:
        """Gets the objective coefficient for the quadratic term associated to the product between two variables.

        The ordering of the input variables does not matter.

        Args:
          first_variable_id: The first variable in the product.
          second_variable_id: The second variable in the product.

        Returns:
          The value of the coefficient.
        """
        return self._coefficients.get(
            _QuadraticKey(first_variable_id, second_variable_id), 0.0
        )


class HashModelStorage(model_storage.ModelStorage):
    """A simple, pure python implementation of ModelStorage.

    Attributes:
        _linear_constraint_matrix: A dictionary with (linear_constraint_id,
          variable_id) keys and numeric values, representing the matrix A for the
          constraints lb_c <= A*x <= ub_c. Invariant: the values have no zeros.
        linear_objective_coefficient: A dictionary with variable_id keys and
          numeric values, representing the linear terms in the objective.
          Invariant: the values have no zeros.
        _quadratic_objective_coefficients: A data structure containing quadratic
          terms in the objective.
    """

    def __init__(self, name: str = "") -> None:
        super().__init__()
        self._name: str = name
        self.variables: Dict[int, _VariableStorage] = {}
        self.linear_constraints: Dict[int, _LinearConstraintStorage] = {}
        self._linear_constraint_matrix: Dict[Tuple[int, int], float] = {}  #
        self._is_maximize: bool = False
        self._objective_offset: float = 0.0
        self.linear_objective_coefficient: Dict[int, float] = {}
        self._quadratic_objective_coefficients: _QuadraticTermStorage = (
            _QuadraticTermStorage()
        )
        self._next_var_id: int = 0
        self._next_lin_con_id: int = 0
        self._update_trackers: weakref.WeakSet[_UpdateTracker] = weakref.WeakSet()

    @property
    def name(self) -> str:
        return self._name

    def add_variable(self, lb: float, ub: float, is_integer: bool, name: str) -> int:
        var_id = self._next_var_id
        self._next_var_id += 1
        self.variables[var_id] = _VariableStorage(lb, ub, is_integer, name)
        return var_id

    def delete_variable(self, variable_id: int) -> None:
        self._check_variable_id(variable_id)
        variable = self.variables[variable_id]
        # First update the watchers
        for watcher in self._update_trackers:
            if variable_id < watcher.variables_checkpoint:
                watcher.variable_deletes.add(variable_id)
                watcher.variable_lbs.discard(variable_id)
                watcher.variable_ubs.discard(variable_id)
                watcher.variable_integers.discard(variable_id)
                watcher.linear_objective_coefficients.discard(variable_id)
                for (
                    other_variable_id
                ) in self._quadratic_objective_coefficients.get_adjacent_variables(
                    variable_id
                ):
                    key = _QuadraticKey(variable_id, other_variable_id)
                    watcher.quadratic_objective_coefficients.discard(key)
                for lin_con_id in variable.linear_constraint_nonzeros:
                    if lin_con_id < watcher.linear_constraints_checkpoint:
                        watcher.linear_constraint_matrix.discard(
                            (lin_con_id, variable_id)
                        )
        # Then update self.
        for lin_con_id in variable.linear_constraint_nonzeros:
            self.linear_constraints[lin_con_id].variable_nonzeros.remove(variable_id)
            del self._linear_constraint_matrix[(lin_con_id, variable_id)]
        del self.variables[variable_id]
        self.linear_objective_coefficient.pop(variable_id, None)
        self._quadratic_objective_coefficients.delete_variable(variable_id)

    def variable_exists(self, variable_id: int) -> bool:
        return variable_id in self.variables

    def next_variable_id(self) -> int:
        return self._next_var_id

    def set_variable_lb(self, variable_id: int, lb: float) -> None:
        self._check_variable_id(variable_id)
        if lb == self.variables[variable_id].lower_bound:
            return
        self.variables[variable_id].lower_bound = lb
        for watcher in self._update_trackers:
            if variable_id < watcher.variables_checkpoint:
                watcher.variable_lbs.add(variable_id)

    def set_variable_ub(self, variable_id: int, ub: float) -> None:
        self._check_variable_id(variable_id)
        if ub == self.variables[variable_id].upper_bound:
            return
        self.variables[variable_id].upper_bound = ub
        for watcher in self._update_trackers:
            if variable_id < watcher.variables_checkpoint:
                watcher.variable_ubs.add(variable_id)

    def set_variable_is_integer(self, variable_id: int, is_integer: bool) -> None:
        self._check_variable_id(variable_id)
        if is_integer == self.variables[variable_id].is_integer:
            return
        self.variables[variable_id].is_integer = is_integer
        for watcher in self._update_trackers:
            if variable_id < watcher.variables_checkpoint:
                watcher.variable_integers.add(variable_id)

    def get_variable_lb(self, variable_id: int) -> float:
        self._check_variable_id(variable_id)
        return self.variables[variable_id].lower_bound

    def get_variable_ub(self, variable_id: int) -> float:
        self._check_variable_id(variable_id)
        return self.variables[variable_id].upper_bound

    def get_variable_is_integer(self, variable_id: int) -> bool:
        self._check_variable_id(variable_id)
        return self.variables[variable_id].is_integer

    def get_variable_name(self, variable_id: int) -> str:
        self._check_variable_id(variable_id)
        return self.variables[variable_id].name

    def get_variables(self) -> Iterator[int]:
        yield from self.variables.keys()

    def add_linear_constraint(self, lb: float, ub: float, name: str) -> int:
        lin_con_id = self._next_lin_con_id
        self._next_lin_con_id += 1
        self.linear_constraints[lin_con_id] = _LinearConstraintStorage(lb, ub, name)
        return lin_con_id

    def delete_linear_constraint(self, linear_constraint_id: int) -> None:
        self._check_linear_constraint_id(linear_constraint_id)
        con = self.linear_constraints[linear_constraint_id]
        # First update the watchers
        for watcher in self._update_trackers:
            if linear_constraint_id < watcher.linear_constraints_checkpoint:
                watcher.linear_constraint_deletes.add(linear_constraint_id)
                watcher.linear_constraint_lbs.discard(linear_constraint_id)
                watcher.linear_constraint_ubs.discard(linear_constraint_id)
                for var_id in con.variable_nonzeros:
                    if var_id < watcher.variables_checkpoint:
                        watcher.linear_constraint_matrix.discard(
                            (linear_constraint_id, var_id)
                        )
        # Then update self.
        for var_id in con.variable_nonzeros:
            self.variables[var_id].linear_constraint_nonzeros.remove(
                linear_constraint_id
            )
            del self._linear_constraint_matrix[(linear_constraint_id, var_id)]
        del self.linear_constraints[linear_constraint_id]

    def linear_constraint_exists(self, linear_constraint_id: int) -> bool:
        return linear_constraint_id in self.linear_constraints

    def next_linear_constraint_id(self) -> int:
        return self._next_lin_con_id

    def set_linear_constraint_lb(self, linear_constraint_id: int, lb: float) -> None:
        self._check_linear_constraint_id(linear_constraint_id)
        if lb == self.linear_constraints[linear_constraint_id].lower_bound:
            return
        self.linear_constraints[linear_constraint_id].lower_bound = lb
        for watcher in self._update_trackers:
            if linear_constraint_id < watcher.linear_constraints_checkpoint:
                watcher.linear_constraint_lbs.add(linear_constraint_id)

    def set_linear_constraint_ub(self, linear_constraint_id: int, ub: float) -> None:
        self._check_linear_constraint_id(linear_constraint_id)
        if ub == self.linear_constraints[linear_constraint_id].upper_bound:
            return
        self.linear_constraints[linear_constraint_id].upper_bound = ub
        for watcher in self._update_trackers:
            if linear_constraint_id < watcher.linear_constraints_checkpoint:
                watcher.linear_constraint_ubs.add(linear_constraint_id)

    def get_linear_constraint_lb(self, linear_constraint_id: int) -> float:
        self._check_linear_constraint_id(linear_constraint_id)
        return self.linear_constraints[linear_constraint_id].lower_bound

    def get_linear_constraint_ub(self, linear_constraint_id: int) -> float:
        self._check_linear_constraint_id(linear_constraint_id)
        return self.linear_constraints[linear_constraint_id].upper_bound

    def get_linear_constraint_name(self, linear_constraint_id: int) -> str:
        self._check_linear_constraint_id(linear_constraint_id)
        return self.linear_constraints[linear_constraint_id].name

    def get_linear_constraints(self) -> Iterator[int]:
        yield from self.linear_constraints.keys()

    def set_linear_constraint_coefficient(
        self, linear_constraint_id: int, variable_id: int, value: float
    ) -> None:
        self._check_linear_constraint_id(linear_constraint_id)
        self._check_variable_id(variable_id)
        if value == self._linear_constraint_matrix.get(
            (linear_constraint_id, variable_id), 0.0
        ):
            return
        if value == 0.0:
            self._linear_constraint_matrix.pop(
                (linear_constraint_id, variable_id), None
            )
            self.variables[variable_id].linear_constraint_nonzeros.discard(
                linear_constraint_id
            )
            self.linear_constraints[linear_constraint_id].variable_nonzeros.discard(
                variable_id
            )
        else:
            self._linear_constraint_matrix[(linear_constraint_id, variable_id)] = value
            self.variables[variable_id].linear_constraint_nonzeros.add(
                linear_constraint_id
            )
            self.linear_constraints[linear_constraint_id].variable_nonzeros.add(
                variable_id
            )
        for watcher in self._update_trackers:
            if (
                variable_id < watcher.variables_checkpoint
                and linear_constraint_id < watcher.linear_constraints_checkpoint
            ):
                watcher.linear_constraint_matrix.add(
                    (linear_constraint_id, variable_id)
                )

    def get_linear_constraint_coefficient(
        self, linear_constraint_id: int, variable_id: int
    ) -> float:
        self._check_linear_constraint_id(linear_constraint_id)
        self._check_variable_id(variable_id)
        return self._linear_constraint_matrix.get(
            (linear_constraint_id, variable_id), 0.0
        )

    def get_linear_constraints_with_variable(self, variable_id: int) -> Iterator[int]:
        self._check_variable_id(variable_id)
        yield from self.variables[variable_id].linear_constraint_nonzeros

    def get_variables_for_linear_constraint(
        self, linear_constraint_id: int
    ) -> Iterator[int]:
        self._check_linear_constraint_id(linear_constraint_id)
        yield from self.linear_constraints[linear_constraint_id].variable_nonzeros

    def get_linear_constraint_matrix_entries(
        self,
    ) -> Iterator[model_storage.LinearConstraintMatrixIdEntry]:
        for (constraint, variable), coef in self._linear_constraint_matrix.items():
            yield model_storage.LinearConstraintMatrixIdEntry(
                linear_constraint_id=constraint,
                variable_id=variable,
                coefficient=coef,
            )

    def clear_objective(self) -> None:
        for variable_id in self.linear_objective_coefficient:
            for watcher in self._update_trackers:
                if variable_id < watcher.variables_checkpoint:
                    watcher.linear_objective_coefficients.add(variable_id)
        self.linear_objective_coefficient.clear()
        for key in self._quadratic_objective_coefficients.keys():
            for watcher in self._update_trackers:
                if key.id2 < watcher.variables_checkpoint:
                    watcher.quadratic_objective_coefficients.add(key)
        self._quadratic_objective_coefficients.clear()
        self.set_objective_offset(0.0)

    def set_linear_objective_coefficient(self, variable_id: int, value: float) -> None:
        self._check_variable_id(variable_id)
        if value == self.linear_objective_coefficient.get(variable_id, 0.0):
            return
        if value == 0.0:
            self.linear_objective_coefficient.pop(variable_id, None)
        else:
            self.linear_objective_coefficient[variable_id] = value
        for watcher in self._update_trackers:
            if variable_id < watcher.variables_checkpoint:
                watcher.linear_objective_coefficients.add(variable_id)

    def get_linear_objective_coefficient(self, variable_id: int) -> float:
        self._check_variable_id(variable_id)
        return self.linear_objective_coefficient.get(variable_id, 0.0)

    def get_linear_objective_coefficients(
        self,
    ) -> Iterator[model_storage.LinearObjectiveEntry]:
        for var_id, coef in self.linear_objective_coefficient.items():
            yield model_storage.LinearObjectiveEntry(
                variable_id=var_id, coefficient=coef
            )

    def set_quadratic_objective_coefficient(
        self, first_variable_id: int, second_variable_id: int, value: float
    ) -> None:
        self._check_variable_id(first_variable_id)
        self._check_variable_id(second_variable_id)
        updated = self._quadratic_objective_coefficients.set_coefficient(
            first_variable_id, second_variable_id, value
        )
        if updated:
            for watcher in self._update_trackers:
                if (
                    max(first_variable_id, second_variable_id)
                    < watcher.variables_checkpoint
                ):
                    watcher.quadratic_objective_coefficients.add(
                        _QuadraticKey(first_variable_id, second_variable_id)
                    )

    def get_quadratic_objective_coefficient(
        self, first_variable_id: int, second_variable_id: int
    ) -> float:
        self._check_variable_id(first_variable_id)
        self._check_variable_id(second_variable_id)
        return self._quadratic_objective_coefficients.get_coefficient(
            first_variable_id, second_variable_id
        )

    def get_quadratic_objective_coefficients(
        self,
    ) -> Iterator[model_storage.QuadraticEntry]:
        yield from self._quadratic_objective_coefficients.coefficients()

    def get_quadratic_objective_adjacent_variables(
        self, variable_id: int
    ) -> Iterator[int]:
        self._check_variable_id(variable_id)
        yield from self._quadratic_objective_coefficients.get_adjacent_variables(
            variable_id
        )

    def set_is_maximize(self, is_maximize: bool) -> None:
        if self._is_maximize == is_maximize:
            return
        self._is_maximize = is_maximize
        for watcher in self._update_trackers:
            watcher.objective_direction = True

    def get_is_maximize(self) -> bool:
        return self._is_maximize

    def set_objective_offset(self, offset: float) -> None:
        if self._objective_offset == offset:
            return
        self._objective_offset = offset
        for watcher in self._update_trackers:
            watcher.objective_offset = True

    def get_objective_offset(self) -> float:
        return self._objective_offset

    def export_model(self) -> model_pb2.ModelProto:
        m: model_pb2.ModelProto = model_pb2.ModelProto()
        m.name = self._name
        _variables_to_proto(self.variables.items(), m.variables)
        _linear_constraints_to_proto(
            self.linear_constraints.items(), m.linear_constraints
        )
        m.objective.maximize = self._is_maximize
        m.objective.offset = self._objective_offset
        if self.linear_objective_coefficient:
            obj_ids, obj_coefs = zip(*sorted(self.linear_objective_coefficient.items()))
            m.objective.linear_coefficients.ids.extend(obj_ids)
            m.objective.linear_coefficients.values.extend(obj_coefs)
        if self._quadratic_objective_coefficients:
            first_var_ids, second_var_ids, coefficients = zip(
                *sorted(
                    [
                        (entry.id_key.id1, entry.id_key.id2, entry.coefficient)
                        for entry in self._quadratic_objective_coefficients.coefficients()
                    ]
                )
            )
            m.objective.quadratic_coefficients.row_ids.extend(first_var_ids)
            m.objective.quadratic_coefficients.column_ids.extend(second_var_ids)
            m.objective.quadratic_coefficients.coefficients.extend(coefficients)
        if self._linear_constraint_matrix:
            flat_matrix_items = [
                (con_id, var_id, coef)
                for ((con_id, var_id), coef) in self._linear_constraint_matrix.items()
            ]
            lin_con_ids, var_ids, lin_con_coefs = zip(*sorted(flat_matrix_items))
            m.linear_constraint_matrix.row_ids.extend(lin_con_ids)
            m.linear_constraint_matrix.column_ids.extend(var_ids)
            m.linear_constraint_matrix.coefficients.extend(lin_con_coefs)
        return m

    def add_update_tracker(self) -> model_storage.StorageUpdateTracker:
        tracker = _UpdateTracker(self)
        self._update_trackers.add(tracker)
        return tracker

    def remove_update_tracker(
        self, tracker: model_storage.StorageUpdateTracker
    ) -> None:
        self._update_trackers.remove(tracker)
        tracker.retired = True

    def _check_variable_id(self, variable_id: int) -> None:
        if variable_id not in self.variables:
            raise model_storage.BadVariableIdError(variable_id)

    def _check_linear_constraint_id(self, linear_constraint_id: int) -> None:
        if linear_constraint_id not in self.linear_constraints:
            raise model_storage.BadLinearConstraintIdError(linear_constraint_id)


def _set_sparse_double_vector(
    id_value_pairs: Iterable[Tuple[int, float]],
    proto: sparse_containers_pb2.SparseDoubleVectorProto,
) -> None:
    """id_value_pairs must be sorted, proto is filled."""
    if not id_value_pairs:
        return
    ids, values = zip(*id_value_pairs)
    proto.ids[:] = ids
    proto.values[:] = values


def _set_sparse_bool_vector(
    id_value_pairs: Iterable[Tuple[int, bool]],
    proto: sparse_containers_pb2.SparseBoolVectorProto,
) -> None:
    """id_value_pairs must be sorted, proto is filled."""
    if not id_value_pairs:
        return
    ids, values = zip(*id_value_pairs)
    proto.ids[:] = ids
    proto.values[:] = values


def _variables_to_proto(
    variables: Iterable[Tuple[int, _VariableStorage]],
    proto: model_pb2.VariablesProto,
) -> None:
    """Exports variables to proto."""
    has_named_var = False
    for _, var_storage in variables:
        if var_storage.name:
            has_named_var = True
            break
    for var_id, var_storage in variables:
        proto.ids.append(var_id)
        proto.lower_bounds.append(var_storage.lower_bound)
        proto.upper_bounds.append(var_storage.upper_bound)
        proto.integers.append(var_storage.is_integer)
        if has_named_var:
            proto.names.append(var_storage.name)


def _linear_constraints_to_proto(
    linear_constraints: Iterable[Tuple[int, _LinearConstraintStorage]],
    proto: model_pb2.LinearConstraintsProto,
) -> None:
    """Exports variables to proto."""
    has_named_lin_con = False
    for _, lin_con_storage in linear_constraints:
        if lin_con_storage.name:
            has_named_lin_con = True
            break
    for lin_con_id, lin_con_storage in linear_constraints:
        proto.ids.append(lin_con_id)
        proto.lower_bounds.append(lin_con_storage.lower_bound)
        proto.upper_bounds.append(lin_con_storage.upper_bound)
        if has_named_lin_con:
            proto.names.append(lin_con_storage.name)
