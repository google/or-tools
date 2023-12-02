#include <string_view>

#include "ortools/base/file.h"

constexpr std::string_view kHelloFile = "/tmp/hello_file.txt";

int main(int argc, char** argv) {
  absl::Status s;
  s = file::SetContents(kHelloFile, "hello file", file::Defaults());
  if (!s.ok()) {
    LOG(ERROR) << "SetContents failed: " << s;
    return 1;
  }

  std::string out;
  s = file::GetContents(kHelloFile, &out, file::Defaults());
  if (!s.ok()) {
    LOG(ERROR) << "GetContents failed: " << s;
    return 1;
  }

  LOG(INFO) << "got back contents: " << out;
  return 0;
}
