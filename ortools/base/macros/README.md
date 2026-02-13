This folder contain freestanding header only files that contain preprocessor
definitions for conditional compilation of parts of the source code. These
constructs are brittle and can fail silently if names are changed. To mitigate
these errors, we recommend adhering to the following guidelines.

Here is a classical example of selectively compilation.

```c++ {.bad}
#include "third_party/ortools/ortools/base/macros/os_support.h"

absl::Status WriteFile(std::string_view filename, std::string_view content) {
#if defined(ORTOOLS_TARGET_OS_SUPPORTS_FILE)
  // Implementation
#else
  return absl::UnavailableError("WriteFile is unsupported on this platform");
#endif // ORTOOLS_TARGET_OS_SUPPORTS_FILE
}
```

There are several ways in which this code may break

 - The `#include` statement is missing,
 - `ORTOOLS_TARGET_OS_SUPPORTS_FILE` is renamed,
 - A typo is introduced (e.g., `ORTOOLS_TARGET_OS_SUPPORTS_FILEE`)

There is no way to prevent such breakage with the preprocessor only so we double
each preprocessor definition with a C++ variable.

A completely safe usage would be as follows.

```c++ {.good}
#include "third_party/ortools/ortools/base/macros/os_support.h"

absl::Status WriteFile(std::string_view filename, std::string_view content) {
#if defined(ORTOOLS_TARGET_OS_SUPPORTS_FILE)
  static_assert(operations_research::kTargetOsSupportsFile);
  // Implementation
#else
  static_assert(!operations_research::kTargetOsSupportsFile);
  return absl::UnavailableError("WriteFile is unsupported on this platform");
#endif // ORTOOLS_TARGET_OS_SUPPORTS_FILE
}
```

In any of the cases above a compilation error would manifest because C++ will
complain is the variable is not defined or is in disagreement with the
preprocessor definition.

Note that you need to always have the `#else` clause and the associated
`static_assert` to get the full protection.
