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

"""Transform any Python sample or example to Python NoteBook."""
import ast
import os
import re
import sys

from nbformat import v3
from nbformat import v4

input_file = sys.argv[1]
print(f"reading {input_file}")
with open(input_file, encoding="utf-8") as fpin:
    text = fpin.read()

# Compute output file path.
output_file = input_file
output_file = output_file.replace(".py", ".ipynb")
# For example/python/foo.py -> example/notebook/examples/foo.ipynb
output_file = output_file.replace("examples/python", "examples/notebook/examples")
# For example/contrib/foo.py -> example/notebook/contrib/foo.ipynb
output_file = output_file.replace("examples/contrib", "examples/notebook/contrib")
# For ortools/*/samples/foo.py -> example/notebook/*/foo.ipynb
output_file = output_file.replace("ortools", "examples/notebook")
output_file = output_file.replace("samples/", "")

nbook = v3.reads_py("")
nbook = v4.upgrade(nbook)  # Upgrade v3 to v4

METADATA = {"language_info": {"name": "python"}}
nbook["metadata"] = METADATA

print("Adding copyright cell...")
GOOGLE = "##### Copyright 2024 Google LLC."
nbook["cells"].append(v4.new_markdown_cell(source=GOOGLE, id="google"))

print("Adding license cell...")
APACHE = """Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
"""
nbook["cells"].append(v4.new_markdown_cell(source=APACHE, id="apache"))

print("Adding Title cell...")
basename = "# " + os.path.basename(input_file).replace(".py", "")
nbook["cells"].append(v4.new_markdown_cell(source=basename, id="basename"))

print("Adding link cell...")
GITHUB_LOGO = (
    "https://raw.githubusercontent.com/google/or-tools/main/tools/github_32px.png"
)
GITHUB_PATH = "https://github.com/google/or-tools/blob/main/" + input_file

COLAB_PATH = (
    "https://colab.research.google.com/github/google/or-tools/blob/main/" + output_file
)
COLAB_LOGO = (
    "https://raw.githubusercontent.com/google/or-tools/main/tools/colab_32px.png"
)
link = f"""<table align=\"left\">
<td>
<a href=\"{COLAB_PATH}\"><img src=\"{COLAB_LOGO}\"/>Run in Google Colab</a>
</td>
<td>
<a href=\"{GITHUB_PATH}\"><img src=\"{GITHUB_LOGO}\"/>View source on GitHub</a>
</td>
</table>"""
nbook["cells"].append(v4.new_markdown_cell(source=link, id="link"))

print("Adding ortools install cell...")
INSTALL_DOC = (
    "First, you must install "
    "[ortools](https://pypi.org/project/ortools/) package in this "
    "colab."
)
nbook["cells"].append(v4.new_markdown_cell(source=INSTALL_DOC, id="doc"))
INSTALL_CMD = "%pip install ortools"
nbook["cells"].append(v4.new_code_cell(source=INSTALL_CMD, id="install"))

print("Adding code cell...")
all_blocks = ast.parse(text).body
print(f"number of blocks: {len(all_blocks)}")
line_start = [c.lineno - 1 for c in all_blocks]
line_start[0] = 0
lines = text.split("\n")

FULL_TEXT = ""
for idx, (c_block, s, e) in enumerate(
    zip(all_blocks, line_start, line_start[1:] + [len(lines)])
):
    print(f"block[{idx}]: {c_block}")
    c_text = "\n".join(lines[s:e])
    # Clean boilerplate header and description
    if (
        idx == 0
        and isinstance(c_block, ast.Expr)
        and isinstance(c_block.value, ast.Constant)
    ):
        print("Adding description cell...")
        filtered_lines = lines[s:e]
        # filtered_lines = list(
        #    filter(lambda l: not l.startswith('#!'), lines[s:e]))
        filtered_lines = list(
            filter(lambda l: not re.search(r"^#!", l), filtered_lines)
        )
        filtered_lines = list(
            filter(lambda l: not re.search(r"# \[START .*\]$", l), filtered_lines)
        )
        filtered_lines = list(
            filter(lambda l: not re.search(r"# \[END .*\]$", l), filtered_lines)
        )
        # TODO(user): Remove only copyright not all line with '^#'
        filtered_lines = list(filter(lambda l: not l.startswith(r"#"), filtered_lines))
        filtered_lines = [s.replace(r'"""', "") for s in filtered_lines]
        filtered_text = "\n".join(filtered_lines)
        nbook["cells"].append(
            v4.new_markdown_cell(source=filtered_text, id="description")
        )
    # Remove absl app import
    elif (
        isinstance(c_block, ast.ImportFrom)
        and c_block.module == "absl"
        and c_block.names[0].name == "app"
    ):
        print(f"Removing import {c_block.module}.{c_block.names[0].name}...")
    # rewrite absl flag import
    elif (
        isinstance(c_block, ast.ImportFrom)
        and c_block.module == "absl"
        and c_block.names[0].name == "flags"
    ):
        print(f"Rewrite import {c_block.module}.{c_block.names[0].name}...")
        FULL_TEXT += "from ortools.sat.colab import flags\n"
    # Unwrap __main__ function
    elif isinstance(c_block, ast.If) and c_block.test.comparators[0].s == "__main__":
        print("Unwrapping main function...")
        c_lines = lines[s + 1 : e]
        # remove start and de-indent lines
        spaces_to_delete = c_block.body[0].col_offset
        fixed_lines = [
            (
                n_line[spaces_to_delete:]
                if n_line.startswith(" " * spaces_to_delete)
                else n_line
            )
            for n_line in c_lines
        ]
        filtered_lines = fixed_lines
        filtered_lines = list(
            filter(lambda l: not re.search(r"# \[START .*\]$", l), filtered_lines)
        )
        filtered_lines = list(
            filter(lambda l: not re.search(r"# \[END .*\]$", l), filtered_lines)
        )
        filtered_lines = [
            re.sub(r"app.run\((.*)\)$", r"\1()", s) for s in filtered_lines
        ]
        FULL_TEXT += "\n".join(filtered_lines) + "\n"
    # Others
    else:
        print("Appending block...")
        filtered_lines = lines[s:e]
        for i, line in enumerate(filtered_lines):
            filtered_lines[i] = line.replace("DEFINE_", "define_")
        filtered_lines = list(
            filter(lambda l: not re.search(r"# \[START .*\]$", l), filtered_lines)
        )
        filtered_lines = list(
            filter(lambda l: not re.search(r"# \[END .*\]$", l), filtered_lines)
        )
        FULL_TEXT += "\n".join(filtered_lines) + "\n"

nbook["cells"].append(
    v4.new_code_cell(source=FULL_TEXT, id="code")
)

jsonform = v4.writes(nbook) + "\n"

print(f"writing {output_file}")
with open(output_file, mode="w", encoding="utf-8") as fpout:
    fpout.write(jsonform)
