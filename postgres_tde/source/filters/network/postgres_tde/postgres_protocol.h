#pragma once

#include "postgres_tde/source/filters/network/postgres_tde/postgres_message.h"

namespace Envoy {
namespace Extensions {
namespace NetworkFilters {
namespace PostgresTDE {

class QueryMessage: public TypedMessage<'Q', String> {
public:
  // Inherit constructors
  using TypedMessage::TypedMessage;

  auto& queryString() {
    return value<0>().value();
  }
};

using ParseMessage = TypedMessage<'P', String, String, Array<Int32>>;

class ColumnDescription : public Sequence<String, Int32, Int16, Int32, Int16, Int32, Int16> {
public:
  // Inherit constructors
  using Sequence::Sequence;

  auto& name() {
    return value<0>().value();
  }

  auto& dataType() {
    return value<3>().value();
  }

  auto& formatCode() {
    return value<6>().value();
  }
};

class RowDescriptionMessage : public TypedMessage<'T', Array<ColumnDescription>> {
public:
  // Inherit constructors
  using TypedMessage::TypedMessage;

  auto& column_descriptions() {
    return value<0>().value();
  }
};

class DataRowMessage: public TypedMessage<'D', Array<VarByteN>> {
public:
  // Inherit constructors
  using TypedMessage::TypedMessage;

  auto& columns() {
    return value<0>().value();
  }
};

using CommandCompleteMessage = TypedMessage<'C', String>;
using EmptyQueryResponseMessage = TypedMessage<'I'>;
using ErrorResponseMessage = TypedMessage<'E', Repeated<Sequence<Byte1, String>>, Byte1>;

using ReadyForQueryMessage = TypedMessage<'Z', Byte1>;

const inline ReadyForQueryMessage READY_FOR_QUERY_MESSAGE(Byte1('I'));

ErrorResponseMessage createErrorResponseMessage(std::string error);

} // namespace PostgresTDE
} // namespace NetworkFilters
} // namespace Extensions
} // namespace Envoy
