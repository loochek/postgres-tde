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

  virtual ColumnConfig* getColumnConfig(std::string table, std::string name) PURE;
};

using DatabaseEncryptionConfigPtr = std::unique_ptr<DatabaseEncryptionConfig>;

} // namespace PostgresTDE
} // namespace NetworkFilters
} // namespace Extensions
} // namespace Envoy
