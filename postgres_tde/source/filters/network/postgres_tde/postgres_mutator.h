#pragma once
#include <cstdint>

#include "envoy/common/platform.h"

#include "source/common/buffer/buffer_impl.h"
#include "source/common/common/logger.h"

#include "absl/container/flat_hash_map.h"
#include "postgres_tde/source/common/sqlutils/sqlutils.h"
#include "postgres_tde/source/filters/network/postgres_tde/postgres_message.h"
#include "postgres_tde/source/filters/network/postgres_tde/postgres_session.h"

namespace Envoy {
namespace Extensions {
namespace NetworkFilters {
namespace PostgresTDE {

struct QueryProcessingResult {
  explicit QueryProcessingResult(bool isOk, std::string error = "") {
    this->isOk = isOk;
    this->error = std::move(error);
  }

  static QueryProcessingResult ok;

  static QueryProcessingResult makeError(std::string error) {
    return QueryProcessingResult(false, std::move(error));
  }

  bool isOk;
  std::string error;
};

class MutationManager;

class Mutator {
public:
  virtual ~Mutator() = default;

  virtual QueryProcessingResult mutateQuery(hsql::SQLParserResult&) PURE;
  virtual void mutateResult() PURE;

  MutationManager* getMutationManager() {
    return mutation_manager_;
  }

protected:
  MutationManager* mutation_manager_;
};

using MutatorPtr = std::unique_ptr<Mutator>;

} // namespace PostgresTDE
} // namespace NetworkFilters
} // namespace Extensions
} // namespace Envoy
