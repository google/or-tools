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

"""Utilities for finding the model associated with a variable/constraint etc.

This file is an implementation detail and not part of the MathOpt public API.
"""

from typing import Protocol
from ortools.math_opt.python.elemental import elemental


class FromModel(Protocol):

    __slots__ = ()

    @property
    def elemental(self) -> elemental.Elemental: ...


def model_is_same(e1: FromModel, e2: FromModel) -> None:
    if e1.elemental is not e2.elemental:
        raise ValueError(
            f"Expected two elements from the same model, but observed {e1} from"
            f" model named: '{e1.elemental.model_name!r}', and {e2} from model"
            f" named: '{e2.elemental.model_name!r}'."
        )
