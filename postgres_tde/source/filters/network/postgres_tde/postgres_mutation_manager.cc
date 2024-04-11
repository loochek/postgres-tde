#include "postgres_tde/source/filters/network/postgres_tde/postgres_mutation_manager.h"
#include "postgres_tde/source/filters/network/postgres_tde/mutators/blind_index.h"
#include "postgres_tde/source/filters/network/postgres_tde/mutators/encryption.h"

namespace Envoy {
namespace Extensions {
namespace NetworkFilters {
namespace PostgresTDE {

MutationManagerImpl::MutationManagerImpl(MutationManagerCallbacks* callbacks)
    : error_state_(Result::ok),
      callbacks_(callbacks) {
  // Order is important
  mutator_chain_.push_back(std::make_unique<BlindIndexMutator>(&config_));
  mutator_chain_.push_back(std::make_unique<EncryptionMutator>(&config_));

  dumper_ = std::make_unique<Common::SQLUtils::DumpVisitor>();
}

void PostgresTDE::MutationManagerImpl::processQuery(std::unique_ptr<QueryMessage>& message) {
  ENVOY_LOG(error, "MutationManagerImpl::processQuery - got {}", message->toString());
  ASSERT(error_state_.isOk);

  Result result = processQueryImpl(*message);
  if (!result.isOk) {
    // Consume message and emit error back
    message.reset();
    emitErrorResponse(result);
  }
}

void PostgresTDE::MutationManagerImpl::processParse(std::unique_ptr<ParseMessage>& message) {
  ENVOY_LOG(error, "MutationManagerImpl::processParse - got {}", message->toString());
  message.reset();
  callbacks_->emitBackendMessage(createErrorResponseMessage("postgres_tde: prepared statements are not supported"));
}

void MutationManagerImpl::processRowDescription(std::unique_ptr<RowDescriptionMessage>& message) {
  ENVOY_LOG(error, "MutationManagerImpl::processRowDescription - got {}", message->toString());

  ASSERT(error_state_.isOk);
  for (auto it = mutator_chain_.rbegin(); it != mutator_chain_.rend(); it++) {
    error_state_ = (*it)->mutateRowDescription(*message);
    if (!error_state_.isOk) {
      ENVOY_LOG(warn, "got error while processing RowDescription, result will be discarded: {}", error_state_.error);
      message.reset();
      return;
    }
  }

  ENVOY_LOG(error, "MutationManagerImpl::processRowDescription - after {}", message->toString());
  retent_response_.push_back(std::move(message));
}

void MutationManagerImpl::processDataRow(std::unique_ptr<DataRowMessage>& message) {
  ENVOY_LOG(error, "MutationManagerImpl::processDataRow");

  if (!error_state_.isOk) {
    ENVOY_LOG(warn, "discarding DataRow due to error");
    message.reset();
    return;
  }

  for (auto it = mutator_chain_.rbegin(); it != mutator_chain_.rend(); it++) {
    error_state_ = (*it)->mutateDataRow(*message);
    if (!error_state_.isOk) {
      ENVOY_LOG(warn, "got error while processing DataRow, result will be discarded: {}", error_state_.error);
      message.reset();
      return;
    }
  }

  retent_response_.push_back(std::move(message));
}

void MutationManagerImpl::processCommandComplete(std::unique_ptr<CommandCompleteMessage>& message) {
  ENVOY_LOG(error, "MutationManagerImpl::processCommandComplete - got {}", message->toString());

  if (error_state_.isOk) {
    // Emit renent response
    for (MessagePtr& retent_message: retent_response_) {
      callbacks_->emitBackendMessage(std::move(retent_message));
    }

    callbacks_->emitBackendMessage(std::move(message));
  } else {
    emitErrorResponse(error_state_);
  }

  retent_response_.clear();
}

void MutationManagerImpl::processEmptyQueryResponse(std::unique_ptr<EmptyQueryResponseMessage>&) {
  ENVOY_LOG(error, "MutationManagerImpl::processEmptyQueryResponse");

  ASSERT(error_state_.isOk);
  // Pass through
}

void MutationManagerImpl::processErrorResponse(std::unique_ptr<ErrorResponseMessage>& message) {
  ENVOY_LOG(error, "MutationManagerImpl::processErrorResponse - got {}", message->toString());

  ASSERT(error_state_.isOk);
  // Pass through
}

Result PostgresTDE::MutationManagerImpl::processQueryImpl(QueryMessage& message) {
  std::string& query_str = message.queryString();

  hsql::SQLParserResult parsed_query;
  hsql::SQLParser::parse(query_str, &parsed_query);
  if (!parsed_query.isValid()) {
// TODO: ???
//    // Pass incorrect queries to the backend in order to get a detailed error message
//    ENVOY_LOG(warn, "query passed through because of parse error");
    return Result::makeError("postgres_tde: unable to parse query");
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

void MutationManagerImpl::emitErrorResponse(const Result& result) {
  ASSERT(!result.isOk);
  callbacks_->emitBackendMessage(createErrorResponseMessage(result.error));
  callbacks_->emitBackendMessage(createReadyForQueryMessage());
  error_state_ = Result::ok;
}

} // namespace PostgresTDE
} // namespace NetworkFilters
} // namespace Extensions
} // namespace Envoy
