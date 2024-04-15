#pragma once

#include "include/sqlparser/SQLParser.h"
#include "postgres_tde/source/common/utils/utils.h"
#include "postgres_tde/source/filters/network/postgres_tde/config/column_config.h"

namespace Envoy {
namespace Extensions {
namespace NetworkFilters {
namespace PostgresTDE {

class DatabaseEncryptionConfig {
public:
  virtual ~DatabaseEncryptionConfig() = default;

  virtual const ColumnConfig* getColumnConfig(const std::string& table, const std::string& name) const PURE;
  virtual bool hasTDEEnabled(const std::string& table) const PURE;
  virtual size_t join_key_size() const PURE;
};

using DatabaseEncryptionConfigPtr = std::unique_ptr<DatabaseEncryptionConfig>;

} // namespace PostgresTDE
} // namespace NetworkFilters
} // namespace Extensions
} // namespace Envoy
