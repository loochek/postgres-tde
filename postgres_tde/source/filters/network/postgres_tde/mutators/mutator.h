#pragma once

#include "include/sqlparser/SQLParser.h"
#include "envoy/common/pure.h"
#include "postgres_tde/source/common/utils/utils.h"
#include "postgres_tde/source/filters/network/postgres_tde/postgres_protocol.h"

namespace Envoy {
namespace Extensions {
namespace NetworkFilters {
namespace PostgresTDE {

using Extensions::Common::Utils::Result;

class MutationManager;

class Mutator {
public:
  virtual ~Mutator() = default;

  virtual Result mutateQuery(hsql::SQLParserResult&) PURE;
  virtual Result mutateRowDescription(RowDescriptionMessage&) { return Result::ok; }
  virtual Result mutateDataRow(std::unique_ptr<DataRowMessage>&) { return Result::ok; }

protected:
  explicit Mutator(MutationManager *mgr): mgr_(mgr) {}

protected:
  MutationManager* mgr_;
};

using MutatorPtr = std::unique_ptr<Mutator>;

} // namespace PostgresTDE
} // namespace NetworkFilters
} // namespace Extensions
} // namespace Envoy
