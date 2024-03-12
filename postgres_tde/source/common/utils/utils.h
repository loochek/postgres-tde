#pragma once

#include <string>

namespace Envoy {
namespace Extensions {
namespace Common {
namespace Utils {

struct Result {
  explicit Result(bool isOk, std::string error = "") {
    this->isOk = isOk;
    this->error = std::move(error);
  }

  static Result ok;

  static Result makeError(std::string error) {
    return Result(false, std::move(error));
  }

  bool isOk;
  std::string error;
};

char* makeOwnedCString(const std::string& str);

} // namespace Utils
} // namespace Common
} // namespace Extensions
} // namespace Envoy
