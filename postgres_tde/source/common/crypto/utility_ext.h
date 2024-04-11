#pragma once

#include <cstdint>
#include <vector>

#include "envoy/buffer/buffer.h"
#include "envoy/common/crypto/crypto.h"

#include "source/common/crypto/utility.h"
#include "source/common/singleton/threadsafe_singleton.h"

#include "absl/strings/string_view.h"

#include "postgres_tde/source/common/utils/utils.h"

namespace Envoy {
namespace Extensions {
namespace Common {
namespace Crypto {

using Utils::Result;

class UtilityExt {
public:
  virtual ~UtilityExt() = default;

  virtual std::vector<uint8_t> GenerateAESKey() PURE;
  virtual Result AESEncrypt(const std::vector<uint8_t>& key, absl::string_view plain_data, std::vector<uint8_t>& out) PURE;
  virtual Result AESDecrypt(const std::vector<uint8_t>& key, absl::string_view encrypted_data, std::vector<uint8_t>& out) PURE;
};

using UtilityExtSingleton = InjectableSingleton<UtilityExt>;
using ScopedUtilityExtSingleton = ScopedInjectableLoader<UtilityExt>;

} // namespace Crypto
} // namespace Common
} // namespace Extensions
} // namespace Envoy