#include "postgres_tde/source/common/crypto/utility_ext_impl.h"

#include "source/common/common/assert.h"
#include "source/common/crypto/crypto_impl.h"

#include "absl/container/fixed_array.h"
#include "absl/strings/ascii.h"
#include "absl/strings/str_cat.h"

#include "absl/strings/escaping.h"

#include "openssl/rand.h"

namespace Envoy {
namespace Extensions {
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

Result UtilityExtImpl::AESEncrypt(const std::vector<uint8_t>& key, absl::string_view plain_data,
                                  std::vector<uint8_t>& out) {
  ASSERT(key.size() == AES_256_KEY_LENGTH);

  const EVP_CIPHER* cipher = EVP_aes_256_cbc();
  bssl::ScopedEVP_CIPHER_CTX ctx;

  std::vector<uint8_t> iv(AES_CBC_IV_LENGTH);
  int ok = RAND_bytes(iv.data(), EVP_CIPHER_iv_length(cipher));
  if (ok != 1) {
    return Result::makeError("postgres_tde: encryption failed");
  }

  ok = EVP_EncryptInit_ex(ctx.get(), cipher, NULL, key.data(), iv.data());
  if (ok != 1) {
    return Result::makeError("postgres_tde: encryption failed");
  }

  int max_encrypted_data_size = Utils::max_ciphertext_size(plain_data.size(), EVP_CIPHER_block_size(cipher));
  std::vector<uint8_t> encrypted_data(max_encrypted_data_size);
  int encrypted_data_size = 0;

  int size = 0;
  ok = EVP_EncryptUpdate(ctx.get(), const_cast<uint8_t*>(encrypted_data.data()), &size,
                         reinterpret_cast<const uint8_t*>(plain_data.data()), plain_data.size());
  if (ok != 1) {
    return Result::makeError("postgres_tde: encryption failed");
  }
  encrypted_data_size = size;

  ok = EVP_EncryptFinal_ex(ctx.get(),
                           const_cast<uint8_t*>(encrypted_data.data()) + encrypted_data_size, &size);
  if (ok != 1) {
    return Result::makeError("postgres_tde: encryption failed");
  }
  encrypted_data_size += size;

  ASSERT(encrypted_data_size <= max_encrypted_data_size);
  encrypted_data.resize(encrypted_data_size);

  std::vector<uint8_t> result(std::move(iv));
  result.insert(result.end(), encrypted_data.begin(), encrypted_data.end());

  out = std::move(result);
  return Result::ok;
}

Result UtilityExtImpl::AESDecrypt(const std::vector<uint8_t>& key, absl::string_view cipher_data,
                                  std::vector<uint8_t>& out) {
  ASSERT(key.size() == AES_256_KEY_LENGTH);

  if (cipher_data.size() < AES_CBC_IV_LENGTH) {
    return Result::makeError("postgres_tde: decryption failed");
  }

  const EVP_CIPHER* cipher = EVP_aes_256_cbc();
  bssl::ScopedEVP_CIPHER_CTX ctx;

  // IV is placed at the beginning of cipher_data by AESEncrypt
  int ok = EVP_DecryptInit_ex(ctx.get(), cipher, NULL, key.data(),
                              reinterpret_cast<const uint8_t*>(cipher_data.data()));
  if (ok != 1) {
    return Result::makeError("postgres_tde: decryption failed");
  }

  int encrypted_data_size = cipher_data.size() - AES_CBC_IV_LENGTH;

  std::vector<uint8_t> plain_data(encrypted_data_size);
  int plain_data_size = 0;

  int size = 0;
  ok =
      EVP_DecryptUpdate(ctx.get(), const_cast<uint8_t*>(plain_data.data()), &size,
                        reinterpret_cast<const uint8_t*>(cipher_data.data()) + AES_CBC_IV_LENGTH,
                        encrypted_data_size);
  if (ok != 1) {
    return Result::makeError("postgres_tde: decryption failed");
  }
  plain_data_size = size;

  ok = EVP_DecryptFinal_ex(ctx.get(), const_cast<uint8_t*>(plain_data.data()) + plain_data_size,
                           &size);
  if (ok != 1) {
    return Result::makeError(fmt::format("postgres_tde: decryption failed"));
  }
  plain_data_size += size;

  ASSERT(plain_data_size <= encrypted_data_size);
  plain_data.resize(plain_data_size);
  out = std::move(plain_data);
  return Result::ok;
}

// Register the crypto utility singleton.
static ScopedUtilityExtSingleton* utility_ext_ =
    new ScopedUtilityExtSingleton(std::make_unique<UtilityExtImpl>());

} // namespace Crypto
} // namespace Common
} // namespace Extensions
} // namespace Envoy
