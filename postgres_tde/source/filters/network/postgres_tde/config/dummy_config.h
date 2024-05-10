#pragma once

#include "postgres_tde/source/filters/network/postgres_tde/config/database_encryption_config.h"

/*
CREATE TABLE cities
(
    id           BYTEA     NOT NULL
        PRIMARY KEY,
    id_bi        BYTEA     NOT NULL,
    id_joinkey   BYTEA     NOT NULL,
    name         BYTEA     NOT NULL,
    name_bi      BYTEA     NOT NULL,
    kladr_id     BYTEA     NOT NULL,
    priority     BYTEA,
    created_at   BYTEA     NOT NULL,
    updated_at   BYTEA     NOT NULL,
    timezone     BYTEA
);

CREATE TABLE city2region
(
    id           BYTEA     NOT NULL
        PRIMARY KEY,
    id_joinkey   BYTEA     NOT NULL,
    region       BYTEA     NOT NULL,
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
inline const char* key8 = "HVFNdwhbIciWvlBahtuLEXDvUAmniZPd";
inline const char* key9 = "JsBbxBLAILBLeTRDSzFPAweXdVnXMAIL";
inline const char* bi_key1 = "jILMFdFDTcuVFCbLZsyIJTpVfQkMolyM";
inline const char* bi_key2 = "dqkddOeIPovxhuwwyrEVJcNxcNHZqcea";

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
                    int32_t orig_data_size, bool has_blind_index, std::vector<uint8_t> bi_key,
                    bool has_join) {
    column_name_ = column_name;

    is_encrypted_ = is_encrypted;
    encryption_key_ = std::move(encryption_key);
    orig_data_type_ = orig_data_type;
    orig_data_size_ = orig_data_size;

    has_blind_index_ = has_blind_index;
    bi_column_name_ = column_name + "_bi";
    bi_key_ = std::move(bi_key);

    has_join_ = has_join;
    join_key_column_name_ = column_name + "_joinkey";
  }

  const std::string& columnName() const override {
    return column_name_;
  }

  bool isEncrypted() const override {
    return is_encrypted_;
  }

  const std::vector<uint8_t>& encryptionKey() const override {
    ASSERT(is_encrypted_);
    return encryption_key_;
  }

  int32_t origDataType() const override {
    ASSERT(is_encrypted_);
    return orig_data_type_;
  }

  int16_t origDataSize() const override {
    ASSERT(is_encrypted_);
    return orig_data_size_;
  }

  bool hasBlindIndex() const override {
    return has_blind_index_;
  }
  const std::string& BIColumnName() const override {
    ASSERT(has_blind_index_);
    return bi_column_name_;
  }

  const std::vector<uint8_t>& BIKey() const override {
    ASSERT(has_blind_index_);
    return bi_key_;
  }

  bool hasJoin() const override {
    return has_join_;
  }

  const std::string& joinKeyColumnName() const override {
    ASSERT(has_join_);
    return join_key_column_name_;
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

  bool has_join_{false};
  std::string join_key_column_name_;
};

class DummyConfig : public DatabaseEncryptionConfig {
public:
  DummyConfig() {
    cities_columns_["id"]         = DummyColumnConfig("id",         true,  create_key_vec(key1), 2950, -1, true,  create_key_vec(bi_key1), true);
    cities_columns_["name"]       = DummyColumnConfig("name",       true,  create_key_vec(key2), 1043, -1, true,  create_key_vec(bi_key2), false);
    cities_columns_["kladr_id"]   = DummyColumnConfig("kladr_id",   true,  create_key_vec(key3), 1043, -1, false, empty_key,                    false);
    cities_columns_["priority"]   = DummyColumnConfig("priority",   true,  create_key_vec(key4), 23,    4, false, empty_key,                    false);
    cities_columns_["created_at"] = DummyColumnConfig("created_at", true,  create_key_vec(key5), 1114, -1, false, empty_key,                    false);
    cities_columns_["updated_at"] = DummyColumnConfig("updated_at", true,  create_key_vec(key6), 1114, -1, false, empty_key,                    false);
    cities_columns_["timezone"]   = DummyColumnConfig("timezone",   true,  create_key_vec(key7), 1043, -1, false, empty_key,                    false);

    city2region_columns_["id"]     = DummyColumnConfig("id",        true,  create_key_vec(key8), 2950, -1, false, empty_key,                    true);
    city2region_columns_["region"] = DummyColumnConfig("region",    true,  create_key_vec(key9), 1043, -1, false, empty_key,                    false);
  }

  const ColumnConfig* getColumnConfig(const std::string& table, const std::string& name) const override {
    if (table == "cities" && cities_columns_.find(name) != cities_columns_.end()) {
      return &cities_columns_.at(name);
    } else if (table == "city2region" && city2region_columns_.find(name) != city2region_columns_.end()) {
      return &city2region_columns_.at(name);
    } else {
      return nullptr;
    }
  }

  bool hasTDEEnabled(const std::string& table) const override {
    return table == "cities" || table == "city2region";
  }

  size_t join_key_size() const override {
    return 2;
  }

private:
  std::unordered_map<std::string, DummyColumnConfig> cities_columns_;
  std::unordered_map<std::string, DummyColumnConfig> city2region_columns_;
};

} // namespace PostgresTDE
} // namespace NetworkFilters
} // namespace Extensions
} // namespace Envoy
