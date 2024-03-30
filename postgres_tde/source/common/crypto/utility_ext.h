#pragma once

#include <cstdint>
#include <vector>

#include "envoy/buffer/buffer.h"
#include "envoy/common/crypto/crypto.h"

#include "source/common/crypto/utility.h"
#include "source/common/singleton/threadsafe_singleton.h"

#include "absl/strings/string_view.h"

namespace Envoy {
namespace Common {
namespace Crypto {

class UtilityExt {
public:
  virtual ~UtilityExt() = default;


  virtual std::vector<uint8_t> GenerateAESKey() PURE;
  virtual std::vector<uint8_t> AESEncrypt(const std::vector<uint8_t>& key, absl::string_view plain_data) PURE;
  virtual std::vector<uint8_t> AESDecrypt(const std::vector<uint8_t>& key, absl::string_view encrypted_data) PURE;
};

using UtilityExtSingleton = InjectableSingleton<UtilityExt>;
using ScopedUtilityExtSingleton = ScopedInjectableLoader<UtilityExt>;

} // namespace Crypto
} // namespace Common
} // namespace Envoy