#pragma once

#include "postgres_tde/source/filters/network/postgres_tde/config/database_encryption_config.h"

namespace Envoy {
namespace Extensions {
namespace NetworkFilters {
namespace PostgresTDE {

class DummyColumnConfig : public ColumnConfig {
public:
  DummyColumnConfig() = default;
  DummyColumnConfig(const std::string &column_name, bool is_encrypted, bool has_blind_index) {
    column_name_ = column_name;
    is_encrypted_ = is_encrypted;
    has_blind_index_ = has_blind_index;
    bi_column_name_ = column_name + "_bi";
  }

  const std::string& columnName() const override {
    return column_name_;
  }

  bool isEncrypted() const override {
    return is_encrypted_;
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
  bool has_blind_index_{false};
  std::string bi_column_name_;
  std::vector<uint8_t> bi_key_;
};

class DummyConfig : public DatabaseEncryptionConfig {
public:
  DummyConfig() {
    columns_["id"]         = DummyColumnConfig("id",         false, false);
    columns_["name"]       = DummyColumnConfig("name"      , true,  true);
    columns_["region"]     = DummyColumnConfig("region",     true,  false);
    columns_["kladr_id"]   = DummyColumnConfig("kladr_id",   true,  false);
    columns_["priority"]   = DummyColumnConfig("priority",   true,  false);
    columns_["created_at"] = DummyColumnConfig("created_at", true,  false);
    columns_["updated_at"] = DummyColumnConfig("updated_at", true,  false);
    columns_["timezone"]   = DummyColumnConfig("timezone",   true,  false);
  }

  ColumnConfig* getColumnConfig(const std::string& table, const std::string& name) override {
    if (table == "city_bi_test" && columns_.find(name) != columns_.end()) {
      return &columns_[name];
    } else {
      return nullptr;
    }
  }

  bool hasTDEEnabled(const std::string& table) override {
    return table == "city_bi_test";
  }

private:
  std::unordered_map<std::string, DummyColumnConfig> columns_;
};

} // namespace PostgresTDE
} // namespace NetworkFilters
} // namespace Extensions
} // namespace Envoy
