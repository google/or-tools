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

"""Collection of helpers to manage absl flags in colab."""


class NotebookStringFlag:
    """Stub for absl flag to be used within a jupyter notebook."""

    def __init__(self, name: str, value: str, doc: str):
        self.__name = name
        self.__value = value
        self.__doc__ = doc

    @property
    def value(self) -> str:
        """Returns the value passed at creation."""
        return self.__value

    @property
    def name(self) -> str:
        """Returns the name of the parameter."""
        return self.__name


def define_string(name: str, value: str, doc: str):
    return NotebookStringFlag(name, value, doc)


class NotebookIntFlag:
    """Stub for absl flag to be used within a jupyter notebook."""

    def __init__(self, name: str, value: int, doc: str):
        self.__name = name
        self.__value = value
        self.__doc__ = doc

    @property
    def value(self) -> int:
        """Returns the value passed at creation."""
        return self.__value

    @property
    def name(self) -> str:
        """Returns the name of the parameter."""
        return self.__name


def define_integer(name: str, value: int, doc: str):
    return NotebookIntFlag(name, value, doc)


class NotebookFloatFlag:
    """Stub for absl flag to be used within a jupyter notebook."""

    def __init__(self, name: str, value: float, doc: str):
        self.__name = name
        self.__value = value
        self.__doc__ = doc

    @property
    def value(self) -> float:
        """Returns the value passed at creation."""
        return self.__value

    @property
    def name(self) -> str:
        """Returns the name of the parameter."""
        return self.__name


def define_float(name: str, value: bool, doc: str):
    return NotebookFloatFlag(name, value, doc)


class NotebookBoolFlag:
    """Stub for absl flag to be used within a jupyter notebook."""

    def __init__(self, name: str, value: bool, doc: str):
        self.__name = name
        self.__value = value
        self.__doc__ = doc

    @property
    def value(self) -> bool:
        """Returns the value passed at creation."""
        return self.__value

    @property
    def name(self) -> str:
        """Returns the name of the parameter."""
        return self.__name


def define_bool(name: str, value: bool, doc: str):
    return NotebookBoolFlag(name, value, doc)
