#!/usr/bin/env python3

"""Doxygen pre-filter script for ion.

This filter processes code and adds Doxygen-compatible markup in various places
to enable Doxygen to read the docs more fully.  Unlike some other Doxygen
filters, it is designed to work with Doxygen's newer markdown syntax.

In order to ensure proper syntax coloring of indented code blocks, make sure
there is a blank (commented) line both above and below the block.  For example:

// Comment comment comment.
//
//     int CodeBlock() {
//       Goes here;
//     }
//
// More comment.
"""


import re
import sys


class DoxygenFormatter(object):
  """Transforms lines of a source file to make them doxygen-friendly."""

  ANYWHERE = 'anywhere'
  COMMENT = 'comment'

  def __init__(self, outfile):
    # The file-like object to which we will write lines.
    self.out = outfile

    # A buffer for storing empty lines which we can use later if we need to
    # retroactively insert markup without causing line number offset problems.
    self.empty_line_buffer = []

    # Whether we are currently inside an indented code block.
    self.in_code_block = False

    self.CompileExpressions()

  def CompileExpressions(self):
    """Pre-compiles frequently used regexps for improved performance.

    The regexps are arranged as a list of 3-tuples, where the second value is
    the replacement string (which may include backreferences) and the third
    value is one of the context constants ANYWHERE or COMMENT.  This is a list
    of tuples instead of a dictionary because order matters: earlier regexps
    will be applied first, and the resulting text (not the original) will be
    what is seen by subsequent regexps.
    """
    self.comment_regex = re.compile(r'^\s*//')

    self.substitutions = [
        # Remove copyright lines.
        (re.compile(r'^\s*//\s*[Cc]opyright.*Google.*'), r'', self.ANYWHERE),

        # Remove any comment lines that consist of only punctuation (banners).
        # We only allow a maximum of two spaces before the punctuation so we
        # don't accidentally get rid of code examples with bare braces and
        # whatnot.
        (re.compile(r'(^\s*)//\s{0,2}[-=#/]+$'), r'\1//\n', self.ANYWHERE),

        # If we find something that looks like a list item that is indented four
        # or more spaces, pull it back to the left so doxygen's Markdown engine
        # doesn't treat it like a code block.
        (re.compile(r'(^\s*)//\s{4,}([-\d*].*)'), r'\1 \2', self.COMMENT),

        # Replace TODO(someone) in a comment with @todo (someone)
        (re.compile(r'TODO'), r'@todo ', self.COMMENT),

        # Replace leading 'Note:' or 'Note that' in a comment with @note
        (re.compile(r'(\/\/\s+)Note(?:\:| that)', re.I), r'\1@note',
         self.COMMENT),

        # Replace leading 'Warning:' in a comment with @warning
        (re.compile(r'(\/\/\s+)Warning:', re.I), r'\1@warning', self.COMMENT),

        # Replace leading 'Deprecated' in a comment with @deprecated
        (re.compile(r'(\/\/\s+)Deprecated[^\w\s]*', re.I), r'\1@deprecated',
         self.COMMENT),

        # Replace pipe-delimited parameter names with backtick-delimiters
        (re.compile(r'\|(\w+)\|'), r'`\1`', self.COMMENT),

        # Convert standalone comment lines to Doxygen style.
        (re.compile(r'(^\s*)//(?=[^/])'), r'\1///', self.ANYWHERE),

        # Strip trailing comments from preprocessor directives.
        (re.compile(r'(^#.*)//.*'), r'\1', self.ANYWHERE),

        # Convert remaining trailing comments to doxygen style, unless they are
        # documenting the end of a block.
        (re.compile(r'([^} ]\s+)//(?=[^/])'), r'\1///<', self.ANYWHERE),
    ]

  def Transform(self, line):
    """Performs the regexp transformations defined by self.substitutions.

    Args:
        line: The line to transform.

    Returns:
        The resulting line.
    """
    for (regex, repl, where) in self.substitutions:
      if where is self.COMMENT and not self.comment_regex.match(line):
        return line
      line = regex.sub(repl, line)
    return line

  def AppendToBufferedLine(self, text):
    """Appends text to the last buffered empty line.

    Empty lines are buffered rather than being written out directly.  This lets
    us retroactively rewrite buffered lines to include markup that affects the
    following line, while avoiding the line number offset that would result from
    inserting a line that wasn't in the original source.

    Args:
        text: The text to append to the line.

    Returns:
        True if there was an available empty line to which text could be
        appended, and False otherwise.
    """
    if self.empty_line_buffer:
      last_line = self.empty_line_buffer.pop().rstrip()
      last_line += text + '\n'
      self.empty_line_buffer.append(last_line)
      return True
    else:
      return False

  def ConvertCodeBlock(self, line):
    """Converts any code block that may begin or end on this line.

    Doxygen has (at least) two kinds of code blocks.  Any block indented at
    least four spaces gets formatted as code, but (for some reason) no syntax
    highlighting is applied.  Any block surrounded by "~~~" on both sides is
    also treated as code, but these are syntax highlighted intelligently
    depending on the file type.  We typically write code blocks in the former
    style, but we'd like them to be highlighted, so this function converts them
    to the latter style by adding in the ~~~ lines.

    To make this a bit more complicated, we would really prefer not to insert
    new lines into the file, since that will make the line numbers shown in
    doxygen not match the line numbers in the actual source code.  For this
    reason, we only perform the conversion if at least one "blank" line (empty
    comment line) appears before the start of the code block.  If we get down to
    the bottom of the block and there's no blank line after it, we will be
    forced to add a line, since we can't go back and undo what we already did.

    Args:
        line: The line to process.

    Returns:
        The converted line.
    """
    if not self.in_code_block and re.match(r'\s*///\s{4,}', line):
      if self.AppendToBufferedLine(' ~~~'):
        # If this fails, we'll just leave it un-highlighted.
        self.in_code_block = True
    elif self.in_code_block and not re.match(r'\s*///\s{4,}', line):
      if not self.AppendToBufferedLine(' ~~~'):
        # This is bad.  We don't have a buffered line to use to end the code
        # block, so we'll have to insert one.  This will cause the line
        # numbers to stop matching the original source, unfortunately.
        line = '/// ~~~\n' + line
      self.in_code_block = False
    return line

  def ProcessLine(self, line):
    """Processes a line.

    If the line is an empty line inside a comment, we buffer it for possible
    rewriting later on.  Otherwise, we transform it using our regexps and
    write it (as well as any buffered blank lines) out to the output.

    Args:
        line: The line to process.
    """
    line = self.Transform(line)

    if line.strip() == '///':
      # We may repurpose this empty line later, so don't write it out yet.
      self.empty_line_buffer.append(line)
    else:
      line = self.ConvertCodeBlock(line)
      # Flush the line buffer and write this line as well.
      for buffered_line in self.empty_line_buffer:
        self.out.write(buffered_line)
      self.empty_line_buffer = []
      self.out.write(line)


def main(argv):
  sourcefile = argv[1]
  with open(sourcefile, 'r') as infile:
    formatter = DoxygenFormatter(sys.stdout)
    for line in infile:
      formatter.ProcessLine(line)


if __name__ == '__main__':
  main(sys.argv)
