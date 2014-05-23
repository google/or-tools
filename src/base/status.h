#ifndef OR_TOOLS_BASE_STATUS_H_
#define OR_TOOLS_BASE_STATUS_H_

#include <string>
#include "base/logging.h"
#include "base/join.h"

namespace operations_research {
namespace util {

namespace error {
enum Error {
  INTERNAL = 1,
  INVALID_ARGUMENT = 2,
  DEADLINE_EXCEEDED = 3,
};
}  // namespace error

struct Status {
  enum { OK = 0 };
  Status() : error_code_(OK) {}
  Status(int error_code) : error_code_(error_code) {}  // NOLINT
  Status(int error_code, const std::string& error_message)
      : error_code_(error_code), error_message_(error_message) {}
  Status(const Status& other)
      : error_code_(other.error_code_), error_message_(other.error_message_) {}

  bool ok() const { return error_code_ == OK; }

  std::string ToString() const {
    if (ok()) return "OK";
    return StrCat("ERROR #", error_code_, ": '", error_message_, "'");
  }

 private:
  int error_code_;
  std::string error_message_;
};

}  // namespace util

#define CHECK_OK(status) CHECK_EQ("", (status).ToString())

}  // namespace operations_research

#endif  // OR_TOOLS_BASE_STATUS_H_
