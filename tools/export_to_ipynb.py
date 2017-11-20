from nbformat import v3, v4
import sys

input = sys.argv[1]
print('reading %s' % input)
with open(input) as fpin:
    text = fpin.read()

nbook = v3.reads_py(text)
nbook = v4.upgrade(nbook)  # Upgrade v3 to v4

jsonform = v4.writes(nbook) + "\n"
output = input
output = output.replace('.py', '.ipynb')
output = output.replace('examples/python', 'examples/notebook')
print('writing %s' % output)
with open(output, "w") as fpout:
    fpout.write(jsonform)
