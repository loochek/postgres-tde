#include "postgres_tde/source/filters/network/postgres_tde/postgres_mutation_manager.h"
#include "postgres_tde/source/filters/network/postgres_tde/mutators/blind_index.h"
#include "postgres_tde/source/filters/network/postgres_tde/mutators/encryption.h"

namespace Envoy {
namespace Extensions {
namespace NetworkFilters {
namespace PostgresTDE {

MutationManagerImpl::MutationManagerImpl() {
  mutator_chain_.push_back(std::make_unique<BlindIndexMutator>(&config_));
  mutator_chain_.push_back(std::make_unique<EncryptionMutator>(&config_));
  dumper_ = std::make_unique<Common::SQLUtils::DumpVisitor>();
}

Result PostgresTDE::MutationManagerImpl::processQuery(QueryMessage& message) {
  ENVOY_LOG(error, "MutationManagerImpl::processQuery - got {}", message.toString());

  std::string& query_str = message.queryString();

  hsql::SQLParserResult parsed_query;
  hsql::SQLParser::parse(query_str, &parsed_query);
  if (!parsed_query.isValid()) {
    // Pass incorrect queries to the backend in order to get a detailed error message
    return Result::ok;
  }

  for (MutatorPtr& mutator : mutator_chain_) {
    Result result = mutator->mutateQuery(parsed_query);
    if (!result.isOk) {
      return result;
    }
  }

  Result result = dumper_->visitQuery(parsed_query);
  if (!result.isOk) {
    return result;
  }

  query_str = dumper_->getResult();
  ENVOY_LOG(error, "mutated query message: {}", message.toString());
  return Result::ok;
}

void MutationManagerImpl::processRowDescription(RowDescriptionMessage& message) {
  ENVOY_LOG(error, "MutationManagerImpl::processRowDescription - got {}", message.toString());

  for (auto it = mutator_chain_.rbegin(); it != mutator_chain_.rend(); it++) {
    (*it)->mutateRowDescription(message);
  }
}

void MutationManagerImpl::processDataRow(DataRowMessage& message) {
  ENVOY_LOG(error, "MutationManagerImpl::processDataRow");

  for (auto it = mutator_chain_.rbegin(); it != mutator_chain_.rend(); it++) {
    (*it)->mutateDataRow(message);
  }
}

void MutationManagerImpl::processCommandComplete(CommandCompleteMessage& data) {
  ENVOY_LOG(error, "MutationManagerImpl::processCommandComplete - got {}", data.toString());

  // TODO: reset sfm?
}

void MutationManagerImpl::processEmptyQueryResponse() {
  ENVOY_LOG(error, "MutationManagerImpl::processEmptyQueryResponse");

  // TODO: reset fsm?
}

void MutationManagerImpl::processErrorResponse(ErrorResponseMessage& data) {
  ENVOY_LOG(error, "MutationManagerImpl::processErrorResponse - got {}", data.toString());

  // TODO: reset sfm?
}

} // namespace PostgresTDE
} // namespace NetworkFilters
} // namespace Extensions
} // namespace Envoy
