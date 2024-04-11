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

template <typename To, typename From>
std::unique_ptr<To> dynamic_unique_cast(std::unique_ptr<From>&& p) {
  if (To* cast = dynamic_cast<To*>(p.get())) {
    std::unique_ptr<To> result(cast);
    p.release();
    return result;
  }

  return std::unique_ptr<To>(nullptr);
}

inline size_t max_ciphertext_size(size_t plaintext_size, size_t block_size) {
  return ((plaintext_size + block_size) / block_size) * block_size;
}

} // namespace Utils
} // namespace Common
} // namespace Extensions
} // namespace Envoy
