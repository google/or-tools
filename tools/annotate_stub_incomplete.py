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

# Copyright 2025-2026 Hunter Hogan
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

# This work may be a derivative work of
# https://github.com/hunterhogan/stubFileNotFound/commit/4e3f8c47d1a9fa6551324657333a5385fe50f2e5 and
# protected by the copyright of the original work. I, Hunter Hogan, hold the copyright of the original
# work and license this derivative work under the License as described above.

"""Annotates every unknown type in a stubgen stub as `Incomplete`.

stubgen omits an annotation whenever pybind11 does not name a Python type:
`*args`, `**kwargs`, C++ types without a registered caster, and bare generics
such as `dict`. Type checkers in strict mode report each omission as an
unknown type. `_typeshed.Incomplete` is the typeshed convention for marking
such a type explicitly until the binding provides a precise one.

The stub keeps stubgen's layout: the annotations are inserted into its text.

Usage: annotate_stub_incomplete.py MODULE
where MODULE is the name given to `stubgen -p`, run from stubgen's output
directory.
"""

import ast
import collections
import operator
import pathlib
import sys

INCOMPLETE = "Incomplete"

# Type arguments that complete each bare generic class stubgen emits.
TYPE_ARGUMENTS_INCOMPLETE = {
    "Callable": f"[..., {INCOMPLETE}]",
    "Iterable": f"[{INCOMPLETE}]",
    "Iterator": f"[{INCOMPLETE}]",
    "Mapping": f"[{INCOMPLETE}, {INCOMPLETE}]",
    "MutableMapping": f"[{INCOMPLETE}, {INCOMPLETE}]",
    "MutableSequence": f"[{INCOMPLETE}]",
    "Sequence": f"[{INCOMPLETE}]",
    "dict": f"[{INCOMPLETE}, {INCOMPLETE}]",
    "frozenset": f"[{INCOMPLETE}]",
    "list": f"[{INCOMPLETE}]",
    "set": f"[{INCOMPLETE}]",
    "tuple": f"[{INCOMPLETE}, ...]",
    "type": f"[{INCOMPLETE}]",
}


class StubText:
    """The lines of a stub, which accept insertions at AST positions.

    Insertions into one line must arrive from left to right: each position is
    an offset into the original line, shifted by the earlier insertions.
    """

    def __init__(self, source: bytes) -> None:
        self.lines = source.splitlines(keepends=True)
        self.insertion_lengths: collections.Counter[int] = collections.Counter()
        self.inserted_incomplete = False

    def insert(self, lineno: int, col_offset: int, text: str) -> None:
        line = self.lines[lineno - 1]
        offset = col_offset + self.insertion_lengths[lineno]
        insertion = text.encode()
        self.lines[lineno - 1] = line[:offset] + insertion + line[offset:]
        self.insertion_lengths[lineno] += len(insertion)
        self.inserted_incomplete = self.inserted_incomplete or INCOMPLETE in text

    def rindex(self, lineno: int, end: int, text: bytes) -> int:
        """Locates `text` before `end`, both as offsets into the original line."""
        offset = self.insertion_lengths[lineno]
        return self.lines[lineno - 1].rindex(text, 0, end + offset) - offset


class GenericCompleter(ast.NodeVisitor):
    """Inserts `Incomplete` type arguments after the bare generics of an annotation."""

    def __init__(self, stub_text: StubText) -> None:
        self.stub_text = stub_text

    def visit_Subscript(self, node: ast.Subscript) -> None:
        self.visit(node.slice)

    def visit_Name(self, node: ast.Name) -> None:
        self.complete(node, node.id)

    def visit_Attribute(self, node: ast.Attribute) -> None:
        self.complete(node, node.attr)

    def complete(self, node: ast.expr, name: str) -> None:
        # stubgen writes a name or a dotted name without whitespace or line
        # breaks, so it ends at its start plus its unparsed length.
        if name in TYPE_ARGUMENTS_INCOMPLETE:
            self.stub_text.insert(
                node.lineno,
                node.col_offset + len(ast.unparse(node).encode()),
                TYPE_ARGUMENTS_INCOMPLETE[name],
            )


class IncompleteAnnotator(ast.NodeVisitor):
    """Annotates unannotated parameters and returns, and completes bare generics."""

    def __init__(self, stub_text: StubText) -> None:
        self.stub_text = stub_text
        self.generic_completer = GenericCompleter(stub_text)
        self.in_class = False
        self.implicit_args: list[ast.arg] = []
        self.imported = False

    def visit_ImportFrom(self, node: ast.ImportFrom) -> None:
        self.imported = self.imported or (
            node.module == "_typeshed"
            and INCOMPLETE in map(operator.attrgetter("name"), node.names)
        )

    def visit_ClassDef(self, node: ast.ClassDef) -> None:
        in_class = self.in_class
        self.in_class = True
        self.generic_visit(node)
        self.in_class = in_class

    def visit_FunctionDef(self, node: ast.FunctionDef) -> None:
        is_method = self.in_class and "staticmethod" not in map(
            ast.unparse, node.decorator_list
        )
        self.implicit_args = (
            (node.args.posonlyargs + node.args.args)[:1] if is_method else []
        )
        in_class = self.in_class
        self.in_class = False
        self.visit(node.args)
        self.in_class = in_class
        if node.returns is not None:
            self.generic_completer.visit(node.returns)
        if node.returns is None:
            self.annotate_returns(node)

    def annotate_returns(self, node: ast.FunctionDef) -> None:
        # The signature ends with the last colon before the body, which is on
        # the body's line in stubgen's `def f(...): ...`.
        body = node.body[0]
        colon = self.stub_text.rindex(body.lineno, body.col_offset, b":")
        returns = " -> None" if node.name == "__init__" else f" -> {INCOMPLETE}"
        self.stub_text.insert(body.lineno, colon, returns)

    def visit_arg(self, node: ast.arg) -> None:
        if node.annotation is not None:
            self.generic_completer.visit(node.annotation)
        if node.annotation is None and node not in self.implicit_args:
            self.stub_text.insert(
                node.lineno,
                node.col_offset + len(node.arg.encode()),
                f": {INCOMPLETE}",
            )

    def visit_AnnAssign(self, node: ast.AnnAssign) -> None:
        self.generic_completer.visit(node.annotation)


def annotate_stub(module: str) -> None:
    """Rewrites the stub of `module` with every unknown type as `Incomplete`."""
    stub = pathlib.Path(*module.split(".")).with_suffix(".pyi")
    source = stub.read_bytes()
    stub_text = StubText(source)
    annotator = IncompleteAnnotator(stub_text)
    annotator.visit(ast.parse(source))
    if stub_text.inserted_incomplete and not annotator.imported:
        stub_text.lines.insert(0, f"from _typeshed import {INCOMPLETE}\n".encode())
    stub.write_bytes(b"".join(stub_text.lines))


if __name__ == "__main__":
    annotate_stub(sys.argv[1])
