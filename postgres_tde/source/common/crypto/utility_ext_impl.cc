#include "postgres_tde/source/common/crypto/utility_ext_impl.h"

#include "source/common/common/assert.h"
#include "source/common/crypto/crypto_impl.h"

#include "absl/container/fixed_array.h"
#include "absl/strings/ascii.h"
#include "absl/strings/str_cat.h"

#include "openssl/rand.h"

namespace Envoy {
namespace Common {
namespace Crypto {

static const size_t AES_256_KEY_LENGTH = 32;
static const size_t AES_CBC_IV_LENGTH = 16;

std::vector<uint8_t> UtilityExtImpl::GenerateAESKey() {
  std::vector<uint8_t> key(AES_256_KEY_LENGTH);
  int ok = RAND_bytes(key.data(), AES_256_KEY_LENGTH);
  RELEASE_ASSERT(ok == 1, "Key generation failed");

  return key;
}

std::vector<uint8_t> UtilityExtImpl::AESEncrypt(const std::vector<uint8_t>& key, absl::string_view plain_data) {
  ASSERT(key.size() == AES_256_KEY_LENGTH);

  const EVP_CIPHER* cipher = EVP_aes_256_cbc();
  bssl::ScopedEVP_CIPHER_CTX ctx;

  std::vector<uint8_t> iv(AES_CBC_IV_LENGTH);
  int ok = RAND_bytes(iv.data(), EVP_CIPHER_iv_length(cipher));
  RELEASE_ASSERT(ok == 1, "Encryption failed");

  ok = EVP_EncryptInit_ex(ctx.get(), cipher, NULL, key.data(), iv.data());
  RELEASE_ASSERT(ok == 1, "Encryption failed");

  std::vector<uint8_t> encrypted_data(plain_data.size() * 2);
  int encrypted_data_len = 0;

  int len = 0;
  ok = EVP_EncryptUpdate(ctx.get(), const_cast<uint8_t*>(encrypted_data.data()), &len, reinterpret_cast<const uint8_t*>(plain_data.data()), plain_data.size());
  RELEASE_ASSERT(ok == 1, "Encryption failed");
  encrypted_data_len = len;

  ok = EVP_EncryptFinal_ex(ctx.get(), const_cast<uint8_t*>(encrypted_data.data()) + encrypted_data_len, &len);
  RELEASE_ASSERT(ok == 1, "Encryption failed");
  encrypted_data_len += len;

  encrypted_data.resize(encrypted_data_len);

  std::vector<uint8_t> result(std::move(iv));
  result.insert(result.end(), encrypted_data.begin(), encrypted_data.end());
  return result;
}

std::vector<uint8_t> UtilityExtImpl::AESDecrypt(const std::vector<uint8_t>& key, absl::string_view encrypted_data) {
  ASSERT(key.size() == AES_256_KEY_LENGTH);
  ASSERT(encrypted_data.size() >= AES_CBC_IV_LENGTH);

  const EVP_CIPHER* cipher = EVP_aes_256_cbc();
  bssl::ScopedEVP_CIPHER_CTX ctx;

  // IV is placed at the beginning of cipher_data by AESEncrypt
  int ok = EVP_DecryptInit_ex(ctx.get(), cipher, NULL, key.data(), reinterpret_cast<const uint8_t*>(encrypted_data.data()));
  RELEASE_ASSERT(ok == 1, "Decryption failed");

  std::vector<uint8_t> plain_data(encrypted_data.size());
  int plain_data_len = 0;

  int len = 0;
  ok = EVP_DecryptUpdate(ctx.get(), const_cast<uint8_t*>(plain_data.data()), &len, reinterpret_cast<const uint8_t*>(encrypted_data.data()) + AES_CBC_IV_LENGTH,
                        encrypted_data.size() - AES_CBC_IV_LENGTH);
  RELEASE_ASSERT(ok == 1, "Decryption failed");
  plain_data_len = len;

  ok = EVP_DecryptFinal_ex(ctx.get(), const_cast<uint8_t*>(plain_data.data()) + plain_data_len, &len);
  RELEASE_ASSERT(ok == 1, "Decryption failed");
  plain_data_len += len;

  plain_data.resize(plain_data_len);
  return plain_data;
}

// Register the crypto utility singleton.
static Crypto::ScopedUtilityExtSingleton* utility_ext_ =
    new Crypto::ScopedUtilityExtSingleton(std::make_unique<Crypto::UtilityExtImpl>());

} // namespace Crypto
} // namespace Common
} // namespace Envoy
