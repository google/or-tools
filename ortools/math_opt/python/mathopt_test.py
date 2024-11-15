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

"""Tests for mathopt."""
import inspect
import types
import typing
from typing import Any, List, Set, Tuple

from absl.testing import absltest
from ortools.math_opt.python import callback
from ortools.math_opt.python import expressions
from ortools.math_opt.python import hash_model_storage
from ortools.math_opt.python import init_arguments
from ortools.math_opt.python import mathopt
from ortools.math_opt.python import message_callback
from ortools.math_opt.python import model
from ortools.math_opt.python import model_parameters
from ortools.math_opt.python import model_storage
from ortools.math_opt.python import parameters
from ortools.math_opt.python import result
from ortools.math_opt.python import solution
from ortools.math_opt.python import solve
from ortools.math_opt.python import solver_resources
from ortools.math_opt.python import sparse_containers

# This list does not contain some modules intentionally:
#
# - `remote_solve`: this depends on Stubby and having it in mathopt.py would
#    force all users using MathOpt to depend on Stubby.
#
# - `statistics`: this is not part of the main libraries. In C++ too it is not
#   included by `cpp/math_opt.h`. If we decide to change that, then maybe it
#   would make sense to replace the top-level functions by member functions on
#   the Model.
#
_MODULES_TO_CHECK: List[types.ModuleType] = [
    callback,
    expressions,
    hash_model_storage,
    init_arguments,
    message_callback,
    model,
    model_parameters,
    model_storage,
    parameters,
    result,
    sparse_containers,
    solution,
    solve,
    solver_resources,
]

# Some symbols are not meant to be exported; we exclude them here.
_EXCLUDED_SYMBOLS: Set[Tuple[types.ModuleType, str]] = {
    (solution, "T"),
}

_TYPING_PUBLIC_CONTENT = [
    getattr(typing, name) for name in dir(typing) if not name.startswith("_")
]


def _is_actual_export(v: Any) -> bool:
    if inspect.ismodule(v):
        return False
    if getattr(v, "__module__", None) != typing.__name__:
        return True
    return v not in _TYPING_PUBLIC_CONTENT


def _get_public_api(module: types.ModuleType) -> List[Tuple[str, Any]]:
    tuple_list = inspect.getmembers(module, _is_actual_export)
    return [(name, obj) for name, obj in tuple_list if not name.startswith("_")]


class MathoptTest(absltest.TestCase):

    def test_imports(self) -> None:
        missing_imports: List[str] = []
        for module in _MODULES_TO_CHECK:
            for name, obj in _get_public_api(module):
                if (module, name) in _EXCLUDED_SYMBOLS:
                    continue
                if hasattr(mathopt, name):
                    self.assertIs(
                        getattr(mathopt, name),
                        obj,
                        msg=f"module: {module.__name__} name: {name}",
                    )
                else:
                    # We don't immediately asserts on a missing import so that we can get
                    # the list of all missing ones.
                    missing_imports.append(f"from {module.__name__} import {name}")
        # We can't have \ in an expression inside an f-string.
        nl = "\n"
        self.assertFalse(
            bool(missing_imports),
            msg=f"missing imports:\n{nl.join(missing_imports)}",
        )


if __name__ == "__main__":
    absltest.main()
