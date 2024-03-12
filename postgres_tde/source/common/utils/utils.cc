#include "postgres_tde/source/common/utils/utils.h"

namespace Envoy {
namespace Extensions {
namespace Common {
namespace Utils {

Result Result::ok = Result(true);

char* makeOwnedCString(const std::string& str) {
  char* c_str = static_cast<char*>(calloc(str.size() + 1, sizeof(char)));
  memcpy(c_str, str.data(), str.size() + 1);
  return c_str;
}

} // namespace Utils
} // namespace Common
} // namespace Extensions
} // namespace Envoy
