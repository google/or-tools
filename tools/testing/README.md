# Binary testing

This folder contains facilities to **test executable files**.
We offer two APIs:

*   `bintest`: A simple scripting language to write simple tests such as
    checking execution success or asserting the presence of text or numbers
    within bounds,
*   `py_bintest`: An extension of the unit testing framework that makes it easy
    to invoke the binary under test, extract values from its output, and check
    them within the unittest framework.

## `bintest`

It offers two commands `RUN:` and `CHECK:`.

*   The `RUN:` command executes the binary and asserts it ran successfully. \
    The passed arguments can use the `$(<label>)` construct to refer to binary
    or data files. These labels are initialized from the `named_data` attribute
    of the [BUILD rule](cs/file:third_party/ortools/tools/testing/BUILD).
*   The `CHECK:` command takes one or more matchers and asserts that they all
    match the binary output. \
    If several matchers are passed to a single `CHECK:` command, they need to
    match in the specified order. \
    Several `CHECK:` lines can be used after a call to `RUN:`, they are all
    independently checking the same output and must all succeed.

Empty lines or lines starting with `#` are not taken into account. Other command
will fail with an error message.

### An example

The `print_args` binary is used as an example.

```cpp
// Snippet from tools/testing/print_args.cc
// This program prints a "Hello world!" message and its command-line arguments,
// one per line.
//
// It has a few options:
//  --fail: If present, the program will exit with a non-zero status code.
//  --stderr: This argument and the following ones are printed to stderr instead
//            of stdout.
//  --file=<file>: Prints the content of the file.
//
// Example usage:
//  print_args --fail --stderr arg1 arg2
//
// This will print:
//  Hello world!
//  --fail
// to stdout, and
//  --stderr
//  arg1
//  arg2
// to stderr, and exit with a non-zero status code.
int main(int argc, char** argv) {
  const std::string file_flag("--file=");
  puts("Hello world!");
  int return_code = EXIT_SUCCESS;
  FILE* out_file = stdout;
  for (int i = 1; i < argc; ++i) {
    const std::string arg = argv[i];
    std::string output = arg;
    if (arg == "--fail") {
      return_code = EXIT_FAILURE;
    } else if (arg == "--stderr") {
      out_file = stderr;
    } else if (arg.starts_with(file_flag)) {
      std::ifstream stream(arg.substr(file_flag.size()));
      std::stringstream buffer;
      buffer << stream.rdbuf();
      output = buffer.str();
    }
    fprintf(out_file, "%s\n", output.c_str());
  }
  return return_code;
}
```

Using the `bintest` rule we can test its behavior.

```build
bintest(
    name = "print_args_bintest",
    srcs = ["print_args.bintest"],
    named_data = {
        "print_args": ":print_args",
        "data_file": ":print_args_data.txt",
    },
)
```

```shell
# Snippet from tools/testing/print_args.bintest
# Running the binary with no argument prints "Hello world!"
RUN: $(print_args)
CHECK: "Hello world!"

# Running the binary with arguments also prints the arguments verbatim.
RUN: $(print_args) --value=0.99999999
CHECK: "Hello world!" "--value=@num(1)"

RUN: $(print_args) a=1 b=54.7 c=12
CHECK: "a=@num(>0)" "b=@num(55~1)" "c=@num(>10, <15)"

# Passing a named variable should be expanded, here we pass the binary path to
# itself.
RUN: $(print_args) --input=$(print_args)
CHECK: "print_args"

# Passing quoted strings
RUN: $(print_args) "Double quoted" 'Single quoted'
CHECK: "Hello world!" "Double quoted" "Single quoted"

# Capturing stderr
RUN: $(print_args) --stderr "written to stderr" 2>&1
CHECK: "written to stderr"

# Expanding side data file
RUN: $(print_args) --file=$(data_file)
CHECK: "This file contains side data."
```

## Matchers

Matchers are single or double quoted strings passed to the `CHECK:` command.
They may contain `@num()` directives.

Here are a few examples:

| Matcher                          | Description                                                                                                                            |
| -------------------------------- | -------------------------------------------------------------------------------------------------------------------------------------- |
| `"Hello world!"`                 | Matches `"Hello world!"`.                                                                                                              |
| `"@num()"`                       | Matches a real finite value, e.g., `-1`, `1.2`, `1.23e45`,<br>To match `inf` or `nan` use plain text matchers.                         |
| `"@num(1)"`                      | Matches a number and assert it is `1`,<br>By default, comparisons are approximate `±1e-6`,<br>For exact match use plain text matchers. |
| `"@num(1~0.5)"`                  | Matches a number and assert it is `1±0.5`.                                                                                             |
| `"@num(>1)"`                     | Matches a number and assert it is `>1`,<br>Available operators are `>=`, `>`, `<=` and `<`.                                            |
| `"@num(>=0, <1)"`                | Matches a number and assert it is `>=0` and `<1`.                                                                                      |
| `"Objective value: @num(100~1)"` | Matches the string `"Objective value: "` followed by a real value that is between `99` and `101`.                                      |

NOTE: This framework is intentionally simple and does not support **regex**. \
For more complex scenarios use `py_bintest` below.

## `py_bintest`

Using the `py_bintest` rule we can write Python tests that invoke the binary and
collect their output. The framework provides functions to match the output using
the same language as in the simple bintest scripts, and extract values out of
it. The extracted values can then be tested using the standard testing API.

Similarly to `bintest` above, text of the form `$(<label>)` in the command line
arguments is expanded using the `named_data` attribute of the
[BUILD rule](cs/file:third_party/ortools/tools/testing/BUILD).

```build
py_bintest(
    name = "print_args_test",
    srcs = ["print_args_test.py"],
    named_data = {
        "print_args": ":print_args",
        "data_file": ":print_args_data.txt",
    },
    deps = ["//testing/pybase"], # MOE:replace deps = [requirement("absl-py")],
)
```

Here is a usage example based on the same `print_args` binary.

```python
# Snippet from tools/testing/print_args_test.py
class BinaryTestingPyTest(binary_test.BinaryTestCase):
  """Tests for testing/binary_test.py."""

  def test_run_no_args(self):
    # Running the binary with no argument prints "Hello world!"
    out = self.assert_binary_succeeds("$(print_args)")
    self.assert_extract(out, "Hello world!")

  def test_run_get_single_float(self):
    # Running the binary with arguments also prints the arguments verbatim.
    out = self.assert_binary_succeeds("$(print_args)", "--value=0.99999999")
    value = self.assert_has_line_with_prefixed_number("--value=", out)
    self.assertAlmostEqual(value, 1.0)

  def test_check_text_and_extract_float(self):
    # Running the binary with arguments also prints the arguments verbatim.
    out = self.assert_binary_succeeds("$(print_args)", "--value=0.99999999")
    (value,) = self.assert_extract(out, "Hello world!", "--value=@num()")
    self.assertAlmostEqual(value, 1.0)

  def test_check_extract_several_floats(self):
    # Running the binary with arguments also prints the arguments verbatim.
    out = self.assert_binary_succeeds("$(print_args)", "a=1", "b=54.7", "c=12")
    (a, b, c) = self.assert_extract(out, "a=@num()", "b=@num()", "c=@num()")
    self.assertGreater(a, 0)
    self.assertAlmostEqual(b, 55, delta=1)
    self.assertBetween(c, 10, 15)

  def test_run_named_args(self):
    # Passing a named variable should be expanded
    out = self.assert_binary_succeeds("$(print_args)", "--input=$(print_args)")
    self.assert_extract(out, "print_args")

  def test_refer_to_side_data(self):
    # Passing a named variable should be expanded
    out = self.assert_binary_succeeds("$(print_args)", "--input=$(data_file)")
    self.assert_extract(out, "print_args_data.txt")

  def test_read_side_data(self):
    path = bintest_run_utils.get_path("data_file")
    with open(path, "r") as f:
      self.assertEqual(f.read(), "This file contains side data.")

  def test_capture_stderr(self):
    # Passing "2>&1" allows to also capture the error stream.
    out = self.assert_binary_succeeds(
        "$(print_args)", "--stderr", "error message", "2>&1"
    )
    self.assert_extract(out, "error message")

  def test_failure(self):
    self.assert_binary_fails("$(print_args)", "--fail")


```

NOTE: Python already has all the infrastructure to test values and so the
`extract` command only accepts `num@()` as a placeholder. Contrary to `bintest`
above it will not accept numerical constraints.
