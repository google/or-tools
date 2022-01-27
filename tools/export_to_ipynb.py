#!/usr/bin/env python3
"""Transform any Python sample or example to Python NoteBook."""
import ast
import os
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
google = '##### Copyright 2021 Google LLC.'
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
github_logo = 'https://raw.githubusercontent.com/google/or-tools/master/tools/github_32px.png'
github_path = 'https://github.com/google/or-tools/blob/master/' + input_file

colab_path = 'https://colab.research.google.com/github/google/or-tools/blob/master/' + output_file
colab_logo = 'https://raw.githubusercontent.com/google/or-tools/master/tools/colab_32px.png'
link = f'''<table align=\"left\">
<td>
<a href=\"{colab_path}\"><img src=\"{colab_logo}\"/>Run in Google Colab</a>
</td>
<td>
<a href=\"{github_path}\"><img src=\"{github_logo}\"/>View source on GitHub</a>
</td>
</table>'''
nbook['cells'].append(v4.new_markdown_cell(source=link, id='link'))

print('Installing ortools cell...')
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
for c_block, s, e in zip(all_blocks, line_start, line_start[1:] + [len(lines)]):
  print(c_block)
  c_text = '\n'.join(lines[s:e])
  if isinstance(c_block,
                ast.If) and c_block.test.comparators[0].s == '__main__':
    print('Skip if main', lines[s:e])
  elif isinstance(c_block, ast.FunctionDef) and c_block.name == 'main':
    # remove start and de-indent lines
    c_lines = lines[s + 1:e]
    spaces_to_delete = c_block.body[0].col_offset
    fixed_lines = [
        n_line[spaces_to_delete:]
        if n_line.startswith(' ' * spaces_to_delete) else n_line
        for n_line in c_lines
    ]
    fixed_text = '\n'.join(fixed_lines)
    print('Unwrapping main function')
    full_text += fixed_text
  else:
    print('appending', c_block)
    full_text += c_text + '\n'

nbook['cells'].append(v4.new_code_cell(source=full_text, id='code'))

jsonform = v4.writes(nbook) + '\n'

print(f'writing {output_file}')
with open(output_file, 'w') as fpout:
  fpout.write(jsonform)
