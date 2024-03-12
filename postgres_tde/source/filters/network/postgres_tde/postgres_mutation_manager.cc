#include "postgres_tde/source/filters/network/postgres_tde/postgres_mutation_manager.h"
#include "postgres_tde/source/filters/network/postgres_tde/mutators/blind_index.h"

namespace Envoy {
namespace Extensions {
namespace NetworkFilters {
namespace PostgresTDE {

MutationManagerImpl::MutationManagerImpl() {
  mutator_chain_.push_back(std::make_unique<BlindIndexMutator>(&config_));
  dumper_ = std::make_unique<Common::SQLUtils::DumpVisitor>();
}

Result
PostgresTDE::MutationManagerImpl::processQuery(std::string& query) {
  ENVOY_LOG(error, "MutationManagerImpl::processQuery - got {}", query);

  hsql::SQLParserResult parsed_query;
  hsql::SQLParser::parse(query, &parsed_query);
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

  query = dumper_->getResult();
  ENVOY_LOG(error, "mutated query: {}", query);
  return Result::ok;
}

void MutationManagerImpl::processRowDescription(Buffer::Instance& data) {
  ENVOY_LOG(error, "MutationManagerImpl::processRowDescription - got {}", data.toString());

  // TODO: save some information about result columns
}

void MutationManagerImpl::processDataRow(Buffer::Instance&) {
  ENVOY_LOG(error, "MutationManagerImpl::processDataRow");

  // TODO: do something - save table layout to be accessible by mutators
}

void MutationManagerImpl::processCommandComplete(Buffer::Instance& data) {
  ENVOY_LOG(error, "MutationManagerImpl::processCommandComplete - got {}", data.toString());

  // TODO: reset sfm?
}

void MutationManagerImpl::processEmptyQueryResponse() {
  ENVOY_LOG(error, "MutationManagerImpl::processEmptyQueryResponse");

  // TODO: reset fsm?
}

void MutationManagerImpl::processErrorResponse(Buffer::Instance& data) {
  ENVOY_LOG(error, "MutationManagerImpl::processErrorResponse - got {}", data.toString());

  // TODO: reset sfm?
}

} // namespace PostgresTDE
} // namespace NetworkFilters
} // namespace Extensions
} // namespace Envoy
