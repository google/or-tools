#!/usr/bin/env python3
# Copyright 2010-2022 Google LLC
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
print(f'reading {input_file}')
with open(input_file) as fpin:
  text = fpin.read()

# Compute output file path.
output_file = input_file
output_file = output_file.replace('.py', '.ipynb')
# For example/python/foo.py -> example/notebook/examples/foo.ipynb
output_file = output_file.replace('examples/python',
                                  'examples/notebook/examples')
# For example/contrib/foo.py -> example/notebook/contrib/foo.ipynb
output_file = output_file.replace('examples/contrib',
                                  'examples/notebook/contrib')
# For ortools/*/samples/foo.py -> example/notebook/*/foo.ipynb
output_file = output_file.replace('ortools', 'examples/notebook')
output_file = output_file.replace('samples/', '')

nbook = v3.reads_py('')
nbook = v4.upgrade(nbook)  # Upgrade v3 to v4

print('Adding copyright cell...')
google = '##### Copyright 2022 Google LLC.'
nbook['cells'].append(v4.new_markdown_cell(source=google, id='google'))

print('Adding license cell...')
apache = '''Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
'''
nbook['cells'].append(v4.new_markdown_cell(source=apache, id='apache'))

print('Adding Title cell...')
basename = '# ' + os.path.basename(input_file).replace('.py', '')
nbook['cells'].append(v4.new_markdown_cell(source=basename, id='basename'))

print('Adding link cell...')
github_logo = 'https://raw.githubusercontent.com/google/or-tools/main/tools/github_32px.png'
github_path = 'https://github.com/google/or-tools/blob/main/' + input_file

colab_path = 'https://colab.research.google.com/github/google/or-tools/blob/main/' + output_file
colab_logo = 'https://raw.githubusercontent.com/google/or-tools/main/tools/colab_32px.png'
link = f'''<table align=\"left\">
<td>
<a href=\"{colab_path}\"><img src=\"{colab_logo}\"/>Run in Google Colab</a>
</td>
<td>
<a href=\"{github_path}\"><img src=\"{github_logo}\"/>View source on GitHub</a>
</td>
</table>'''
nbook['cells'].append(v4.new_markdown_cell(source=link, id='link'))

print('Adding ortools install cell...')
install_doc = ('First, you must install '
               '[ortools](https://pypi.org/project/ortools/) package in this '
               'colab.')
nbook['cells'].append(v4.new_markdown_cell(source=install_doc, id='doc'))
install_cmd = '!pip install ortools'
nbook['cells'].append(v4.new_code_cell(source=install_cmd, id='install'))

print('Adding code cell...')
all_blocks = ast.parse(text).body
print(f'number of bocks: {len(all_blocks)}')
line_start = [c.lineno - 1 for c in all_blocks]
line_start[0] = 0
lines = text.split('\n')

full_text = ''
for idx, (c_block, s, e) in enumerate(
        zip(all_blocks, line_start, line_start[1:] + [len(lines)])):
  print(f'block[{idx}]: {c_block}')
  c_text = '\n'.join(lines[s:e])
  # Clean boilerplate header and description
  if (idx == 0 and isinstance(c_block, ast.Expr) and
      isinstance(c_block.value, ast.Constant)):
    print('Adding description cell...')
    filtered_lines = lines[s:e]
    # filtered_lines = list(
    #    filter(lambda l: not l.startswith('#!'), lines[s:e]))
    filtered_lines = list(
        filter(lambda l: not re.search(r'^#!', l), filtered_lines))
    filtered_lines = list(
        filter(lambda l: not re.search(r'# \[START .*\]$', l), filtered_lines))
    filtered_lines = list(
        filter(lambda l: not re.search(r'# \[END .*\]$', l), filtered_lines))
    # TODO(user): Remove only copyright not all line with '^#'
    filtered_lines = list(
        filter(lambda l: not l.startswith(r'#'), filtered_lines))
    filtered_lines = [s.replace(r'"""', '') for s in filtered_lines]
    filtered_text = '\n'.join(filtered_lines)
    nbook['cells'].append(
        v4.new_markdown_cell(source=filtered_text, id='description'))
  # Remove absl app and flags import
  elif (isinstance(c_block, ast.ImportFrom) and c_block.module == 'absl'
        and c_block.names[0].name in ('flags', 'app')):
    print(f'Removing import {c_block.module}.{c_block.names[0].name}...')
  # Rewrite `FLAGS = flags.FLAGS`
  elif (isinstance(c_block, ast.Assign) and
        isinstance(c_block.targets[0], ast.Name) and
        c_block.targets[0].id == 'FLAGS'):
    print('Adding FLAGS class...')
    fixed_lines = ['class FLAGS: pass\n']
    full_text += '\n'.join(fixed_lines) + '\n'
  # Rewrite `flags.DEFINE_*(*)`
  elif (isinstance(c_block, ast.Expr) and
        isinstance(c_block.value, ast.Call) and
        isinstance(c_block.value.func, ast.Attribute) and
        c_block.value.func.value.id == 'flags'):
    print('Adding FLAGS field...')
    fixed_lines = []
    attr = c_block.value.func.attr
    if attr in ('DEFINE_integer', 'DEFINE_bool', 'DEFINE_string'):
      args = c_block.value.args
      # print(f'args: {args}')
      name = args[0].value
      if isinstance(args[1], ast.Constant):
        value = args[1].value
      elif isinstance(args[1], ast.UnaryOp):
        if isinstance(args[1].op, ast.USub):
          value = -1 * int(args[1].operand.value)
        else:
          print(f'unknown value operator: "{args[1].op}"')
          sys.exit(2)
      else:
        print(f'unknown value: "{args[1]}"')
        sys.exit(2)
      comment = args[2].value

      print(f'FLAGS.{name} = \'{value}\' # {comment}')
      if attr in ('DEFINE_integer', 'DEFINE_bool'):
        fixed_lines.append(f'FLAGS.{name} = {value} # {comment}\n')
      else:
        fixed_lines.append(f'FLAGS.{name} = \'{value}\' # {comment}\n')
    else:
      print(f'unknown method: "{attr}"')
      sys.exit(2)
    full_text += '\n'.join(fixed_lines)
    # Add empty line after the last flags.DEFINE
    if e-2 >= s and not lines[e-1] and not lines[e-2]:
      full_text += '\n'
  # Unwrap __main__ function
  elif (isinstance(c_block, ast.If) and
        c_block.test.comparators[0].s == '__main__'):
    print('Unwrapping main function...')
    c_lines = lines[s + 1:e]
    # remove start and de-indent lines
    spaces_to_delete = c_block.body[0].col_offset
    fixed_lines = [
        n_line[spaces_to_delete:]
        if n_line.startswith(' ' * spaces_to_delete) else n_line
        for n_line in c_lines
    ]
    filtered_lines = fixed_lines
    filtered_lines = list(
        filter(lambda l: not re.search(r'# \[START .*\]$', l), filtered_lines))
    filtered_lines = list(
        filter(lambda l: not re.search(r'# \[END .*\]$', l), filtered_lines))
    filtered_lines = [
        re.sub(r'app.run\((.*)\)$', r'\1()', s) for s in filtered_lines
    ]
    full_text += '\n'.join(filtered_lines) + '\n'
  # Others
  else:
    print('Appending block...')
    filtered_lines = lines[s:e]
    filtered_lines = list(
        filter(lambda l: not re.search(r'# \[START .*\]$', l), filtered_lines))
    filtered_lines = list(
        filter(lambda l: not re.search(r'# \[END .*\]$', l), filtered_lines))
    full_text += '\n'.join(filtered_lines) + '\n'

nbook['cells'].append(v4.new_code_cell(source=full_text, id='code'))

jsonform = v4.writes(nbook) + '\n'

print(f'writing {output_file}')
with open(output_file, 'w') as fpout:
  fpout.write(jsonform)
