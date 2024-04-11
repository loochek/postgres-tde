#pragma once

#include "postgres_tde/source/filters/network/postgres_tde/config/database_encryption_config.h"

/*
CREATE TABLE CITY_BI_TEST
(
    id         UUID      NOT NULL
        PRIMARY KEY,
    name       BYTEA     NOT NULL,
    name_bi    BYTEA     NOT NULL,
    region     BYTEA     NOT NULL,
    kladr_id   BYTEA     NOT NULL,
    priority   BYTEA,
    created_at BYTEA     NOT NULL,
    updated_at BYTEA     NOT NULL,
    timezone   BYTEA
);
*/

namespace Envoy {
namespace Extensions {
namespace NetworkFilters {
namespace PostgresTDE {

inline const char* key1 = "ymIuopubnMBxWDGumWqyZYFKyObszrmz";
inline const char* key2 = "TexFFNQaLqYvlWqqqffccVUQirOesMEZ";
inline const char* key3 = "xGPhTvYGHTEYYsiZvOezhcOsgQOQfMWD";
inline const char* key4 = "LNnocxaVcIbLhBoJWcPLzKRhkfUywcLo";
inline const char* key5 = "YlRylyTAGqdjqBOUnpoaZAxJYGwopmAf";
inline const char* key6 = "pFItaovgXxUEhPqeoHbVWyIZsIcxwvbc";
inline const char* key7 = "UtgCnFbaYyhDCdrJqmXsrkAIaGIMvCUS";
inline const char* bi_key = "jILMFdFDTcuVFCbLZsyIJTpVfQkMolyM";

inline const std::vector<uint8_t> empty_key;

inline std::vector<uint8_t> create_key_vec(const char* key) {
  std::vector<uint8_t> vec(32);
  memcpy(vec.data(), key, 32);
  return vec;
}

class DummyColumnConfig : public ColumnConfig {
public:
  DummyColumnConfig() = default;
  DummyColumnConfig(const std::string &column_name, bool is_encrypted,
                    std::vector<uint8_t> encryption_key, int32_t orig_data_type,
                    int32_t orig_data_size, bool has_blind_index, std::vector<uint8_t> bi_key) {
    column_name_ = column_name;
    is_encrypted_ = is_encrypted;
    encryption_key_ = std::move(encryption_key);
    orig_data_type_ = orig_data_type;
    orig_data_size_ = orig_data_size;
    has_blind_index_ = has_blind_index;
    bi_column_name_ = column_name + "_bi";
    bi_key_ = std::move(bi_key);
  }

  const std::string& columnName() const override {
    return column_name_;
  }

  bool isEncrypted() const override {
    return is_encrypted_;
  }

  const std::vector<uint8_t>& encryptionKey() const override {
    assert(is_encrypted_);
    return encryption_key_;
  }

  int32_t origDataType() const override {
    assert(is_encrypted_);
    return orig_data_type_;
  }

  int16_t origDataSize() const override {
    assert(is_encrypted_);
    return orig_data_size_;
  }

  bool hasBlindIndex() const override {
    return has_blind_index_;
  }
  const std::string& BIColumnName() const override {
    assert(has_blind_index_);
    return bi_column_name_;
  }

  const std::vector<uint8_t>& BIKey() const override {
    assert(has_blind_index_);
    return bi_key_;
  }

private:
  std::string column_name_;
  bool is_encrypted_{false};
  std::vector<uint8_t> encryption_key_;
  int32_t orig_data_type_;
  int16_t orig_data_size_;

  bool has_blind_index_{false};
  std::string bi_column_name_;
  std::vector<uint8_t> bi_key_;
};

class DummyConfig : public DatabaseEncryptionConfig {
public:
  DummyConfig() {
    columns_["id"]         = DummyColumnConfig("id",         false, empty_key,                 2950, -1, false, empty_key);
    columns_["name"]       = DummyColumnConfig("name",       true,  create_key_vec(key1), 1043, -1, true,  create_key_vec(bi_key));
    columns_["region"]     = DummyColumnConfig("region",     true,  create_key_vec(key2), 1043, -1, false, empty_key);
    columns_["kladr_id"]   = DummyColumnConfig("kladr_id",   true,  create_key_vec(key3), 1043, -1, false, empty_key);
    columns_["priority"]   = DummyColumnConfig("priority",   true,  create_key_vec(key4), 23,    4, false, empty_key);
    columns_["created_at"] = DummyColumnConfig("created_at", true,  create_key_vec(key5), 1114, -1, false, empty_key);
    columns_["updated_at"] = DummyColumnConfig("updated_at", true,  create_key_vec(key6), 1114, -1, false, empty_key);
    columns_["timezone"]   = DummyColumnConfig("timezone",   true,  create_key_vec(key7), 1043, -1, false, empty_key);
  }

  ColumnConfig* getColumnConfig(const std::string& table, const std::string& name) override {
    if ((table == "city_bi_test" || table == "city_bi_test2") && columns_.find(name) != columns_.end()) {
      return &columns_[name];
    } else {
      return nullptr;
    }
  }

  bool hasTDEEnabled(const std::string& table) override {
    return table == "city_bi_test" || table == "city_bi_test2";
  }

private:
  std::unordered_map<std::string, DummyColumnConfig> columns_;
};

} // namespace PostgresTDE
} // namespace NetworkFilters
} // namespace Extensions
} // namespace Envoy
