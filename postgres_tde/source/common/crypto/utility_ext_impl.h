#pragma once

#include "postgres_tde/source/common/crypto/utility_ext.h"

namespace Envoy {
namespace Common {
namespace Crypto {

class UtilityExtImpl : public Envoy::Common::Crypto::UtilityExt {
public:
  std::vector<uint8_t> GenerateAESKey() override;
  std::vector<uint8_t> AESEncrypt(const std::vector<uint8_t>& key, absl::string_view plain_data) override;
  std::vector<uint8_t> AESDecrypt(const std::vector<uint8_t>& key, absl::string_view encrypted_data) override;
};

} // namespace Crypto
} // namespace Common
} // namespace Envoy