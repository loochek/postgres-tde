#include "postgres_tde/source/filters/network/postgres_tde/mutators/encryption.h"
#include "source/common/crypto/utility.h"
//#include "source/common/common/fmt.h"
//#include "absl/strings/escaping.h"
//#include "absl/cleanup/cleanup.h"

namespace Envoy {
namespace Extensions {
namespace NetworkFilters {
namespace PostgresTDE {

EncryptionMutator::EncryptionMutator(DatabaseEncryptionConfig *config) : config_(config) {}

Result EncryptionMutator::mutateQuery(hsql::SQLParserResult& query) {
//  CHECK_RESULT(Visitor::visitQuery(query));
//  CHECK_RESULT(mutateComparisons());
//  CHECK_RESULT(mutateGroupByExpressions());
//  CHECK_RESULT(mutateInsertStatement());
//  CHECK_RESULT(mutateUpdateStatement());
  return Result::ok;
}

void EncryptionMutator::mutateResult() {}

} // namespace PostgresTDE
} // namespace NetworkFilters
} // namespace Extensions
} // namespace Envoy
