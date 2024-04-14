#pragma once

#include "source/common/common/logger.h"

namespace Envoy {
namespace Extensions {
namespace NetworkFilters {
namespace PostgresTDE {

struct ColumnConfig {
public:
  virtual ~ColumnConfig() = default;

  virtual const std::string& columnName() const PURE;

  // Encryption
  virtual bool isEncrypted() const PURE;
  virtual const std::vector<uint8_t>& encryptionKey() const PURE;
  virtual int32_t origDataType() const PURE;
  virtual int16_t origDataSize() const PURE;

  // Blind index
  virtual bool hasBlindIndex() const PURE;
  virtual const std::string& BIColumnName() const PURE;
  virtual const std::vector<uint8_t>& BIKey() const PURE;

  // Probabilistic join
  virtual bool hasJoin() const PURE;
  virtual const std::string& joinKeyColumnName() const PURE;
};

} // namespace PostgresTDE
} // namespace NetworkFilters
} // namespace Extensions
} // namespace Envoy
