#ifndef OR_TOOLS_BASE_STATUSOR_H_
#define OR_TOOLS_BASE_STATUSOR_H_

#include "base/status.h"

namespace operations_research {
namespace util {

// T should be a pointer type. Eg. StatusOr<Graph*>.
template <class T>
struct StatusOr {
  // Non-explicit constructors, by design.
  StatusOr(T value) : value_(value) {  // NOLINT
    CHECK(value != nullptr);  // This enforces that T is a pointer type.
  }
  StatusOr(const Status& status) : status_(status) {  // NOLINT
    CHECK(!status_.ok()) << status.ToString();
  }

  // Copy constructor.
  StatusOr(const StatusOr& other)
      : value_(other.value_), status_(other.status_) {}

  bool ok() const { return value_ != nullptr; }
  const T& ValueOrDie() const {
    CHECK(ok());
    return value_;
  }

  Status status() const {
    return value_ != nullptr ? /*OK*/ Status() : status_;
  }

 private:
  T value_;
  Status status_;
};

}  // namespace util
}  // namespace operations_research

#endif  // OR_TOOLS_BASE_STATUSOR_H_
