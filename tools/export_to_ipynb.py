#!/usr/bin/env python3
"""Transform any Python sample or example to Python NoteBook."""
import ast
import sys
from nbformat import v3
from nbformat import v4

input_file = sys.argv[1]
print('reading %s' % input_file)
with open(input_file) as fpin:
  text = fpin.read()

nbook = v3.reads_py('')
nbook = v4.upgrade(nbook)  # Upgrade v3 to v4

all_blocks = ast.parse(text).body
line_start = [c.lineno - 1 for c in all_blocks]
line_start[0] = 0
lines = text.split('\n')

full_text = ''

for c_block, s, e in zip(all_blocks, line_start, line_start[1:] + [len(lines)]):
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

nbook['cells'].append(v4.new_code_cell(full_text))

jsonform = v4.writes(nbook) + '\n'
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

print('writing %s' % output_file)
with open(output_file, 'w') as fpout:
  fpout.write(jsonform)
