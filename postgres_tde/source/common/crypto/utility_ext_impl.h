#pragma once

#include "postgres_tde/source/common/crypto/utility_ext.h"

namespace Envoy {
namespace Extensions {
namespace Common {
namespace Crypto {

class UtilityExtImpl : public UtilityExt {
public:
  std::vector<uint8_t> GenerateAESKey() override;
  Result AESEncrypt(const std::vector<uint8_t>& key, absl::string_view plain_data,
                    std::vector<uint8_t>& out) override;
  Result AESDecrypt(const std::vector<uint8_t>& key, absl::string_view cipher_data,
                    std::vector<uint8_t>& out) override;
};


} // namespace Crypto
} // namespace Common
} // namespace Extensions
} // namespace Envoy