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

"""Matchers for the bintest framework."""

from collections.abc import Callable, Sequence
import re


class MatchError(Exception):
    """Exception raised when a match fails."""


def _enclosed(delimiters: str):
    """Creates a regex string to match content enclosed by delimiters.

    Args:
      delimiters: A string of two characters representing the left and right
        delimiters. e.g., "()"

    Returns:
      A regex string.
    """
    assert len(delimiters) == 2
    left = re.escape(delimiters[0])
    right = re.escape(delimiters[1])
    inside = f"[^{right}]*"
    return f"{left}{inside}{right}"


class Re:
    """Regular expression patterns used for parsing."""

    _SQ_STRING = _enclosed('""')
    _DQ_STRING = _enclosed("''")
    STRING = f"({_SQ_STRING}|{_DQ_STRING})"
    NUMBER = r"([-+]?[0-9]*(?:\.[0-9]+)?(?:[eE][-+]?[0-9]+)?)"

    _PARENTHESIZED_EXPR = _enclosed("()")
    _KEYWORD_EXPR = f"(@num{_PARENTHESIZED_EXPR})"
    _KEYWORD_SPEC = f"@num({_PARENTHESIZED_EXPR})"
    _SPEC_ALMOST = f"{NUMBER}(?:~{NUMBER})?"
    _SPEC_BINOP = f"(>=|<=|>|<) *{NUMBER}"
    _SPEC_BINOPS = _SPEC_BINOP + f"(?: *, *{_SPEC_BINOP})?"


_CheckFloatFn = Callable[[float], None]


def _check_almost_equal(lhs: float, rhs: float, delta: float):
    if lhs == rhs:
        return
    diff = abs(lhs - rhs)
    if diff > delta:
        message = f"{lhs} != {rhs} within {delta} delta ({diff} difference)"
        raise MatchError(message)


def _check_less(lhs: float, rhs: float):
    if not lhs < rhs:
        raise MatchError(f"{lhs} not less than {rhs}")


def _check_less_equal(lhs: float, rhs: float):
    if not lhs <= rhs:
        raise MatchError(f"{lhs} not less than or equal to {rhs}")


def _check_greater(lhs: float, rhs: float):
    if not lhs > rhs:
        raise MatchError(f"{lhs} not greater than {rhs}")


def _check_greater_equal(lhs: float, rhs: float):
    if not lhs >= rhs:
        raise MatchError(f"{lhs} not greater than or equal to {rhs}")


def _check_nothing(value: float):
    """A no-operation function for float checks."""
    del value  # Unused


def _parse_binop(op: str, value: float) -> _CheckFloatFn:
    """Parses a binary operator and a value into a check function.

    Args:
      op: The binary operator string (e.g., '<', '>=').
      value: The float value to compare against.

    Returns:
      A function that takes a float and performs the assertion.

    Raises:
      ValueError: If the operator is not supported.
    """
    if op == "<":
        return lambda actual: _check_less(actual, value)
    if op == "<=":
        return lambda actual: _check_less_equal(actual, value)
    if op == ">":
        return lambda actual: _check_greater(actual, value)
    if op == ">=":
        return lambda actual: _check_greater_equal(actual, value)
    raise ValueError(f"Unsupported operator: {op}")


def _parse_spec(spec: str) -> _CheckFloatFn:
    """Parses a specification string into a float checking function.

    The spec can define almost equality (e.g., "1.0~0.1") or binary
    operations (e.g., ">0.0", "<=1.0").

    Args:
      spec: The specification string.

    Returns:
      A function that takes a float and performs the specified checks.

    Raises:
      ValueError: If the spec string is unsupported.
    """
    spec = spec.strip()
    if not spec:
        return _check_nothing
    if match := re.fullmatch(Re._SPEC_ALMOST, spec):
        value, delta = match.groups()
        expected = float(value)
        delta = float(delta) if delta else 1e-6
        return lambda actual: _check_almost_equal(actual, expected, delta)
    if match := re.fullmatch(Re._SPEC_BINOPS, spec):
        op1, value1, op2, value2 = match.groups()
        fn1 = _parse_binop(op1, float(value1))
        fn2 = _parse_binop(op2, float(value2)) if op2 and value2 else None

        def fn(actual: float):
            fn1(actual)
            if fn2:
                fn2(actual)

        return fn
    raise ValueError(f"Unsupported spec: {spec!r}")


def _split_pattern(
    pattern: str, check_matcher_specs: bool
) -> tuple[str, Sequence[_CheckFloatFn]]:
    """Splits a pattern string into a regex and check functions.

    Identifies "@num(spec)" keywords and replaces them with a number regex,
    extracting the spec to create check functions.

    Args:
      pattern: The input pattern string.
      check_matcher_specs: Whether to allow check specifications within @num().

    Returns:
      A tuple containing the generated regex string and a list of check functions.
    """
    regex_pieces = []
    check_functions = []
    for part in re.split(Re._KEYWORD_EXPR, pattern):
        if expr := re.fullmatch(Re._KEYWORD_SPEC, part):
            spec = expr[1][1:-1]
            regex_pieces.append(Re.NUMBER)
            check_fn = _parse_spec(spec)
            if not check_matcher_specs and check_fn is not _check_nothing:
                raise ValueError(f"Constraints are not allowed : {part!r}")
            check_functions.append(check_fn)
        else:
            regex_pieces.append(re.escape(part))
    return "".join(regex_pieces), check_functions


def extract(log: str, *patterns: str, check_matcher_specs: bool) -> Sequence[float]:
    """Extracts and optionally checks numbers from a log string based on patterns.

    Each pattern can contain "@num(spec)" keywords, which will be replaced
    by a regex to capture a floating-point number. The "spec" inside the
    parentheses can be used to specify checks on the extracted number if
    `check_matcher_specs` is True.

    Args:
      log: The log string to search within.
      *patterns: One or more pattern strings to search for sequentially in the
        log.
      check_matcher_specs: If True, specifications within "@num()" will be parsed
        and used to assert conditions on the extracted numbers.

    Returns:
      A tuple containing all the extracted floating-point numbers in order of
      appearance. Returns an empty tuple if no matchers were used in patterns.

    Raises:
      MatchError: If any pattern is not found in the remaining log string, or if
        any check on an extracted number fails.
    """
    floats = []
    unmatched_log = log
    for pattern in patterns:
        regex, check_functions = _split_pattern(pattern, check_matcher_specs)
        match = re.search(regex, unmatched_log)
        if not match:
            raise MatchError(f"No match for {pattern!r}")
        assert len(match.groups()) == len(check_functions)
        for float_str, check_fn in zip(match.groups(), check_functions):
            actual_float = float(float_str)
            check_fn(actual_float)
            floats.append(actual_float)
        unmatched_log = unmatched_log[match.end() :]
    return tuple(floats)
