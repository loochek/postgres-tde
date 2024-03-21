#pragma once

#include "postgres_tde/source/filters/network/postgres_tde/postgres_message.h"

namespace Envoy {
namespace Extensions {
namespace NetworkFilters {
namespace PostgresTDE {

using QueryMessage = TypedMessage<'Q', String>;
using ParseMessage = TypedMessage<'P', String, String, Array<Int32>>;
using RowDescriptionMessage =
    TypedMessage<'T', Array<Sequence<String, Int32, Int16, Int32, Int16, Int32, Int16>>>;
using DataRowMessage = TypedMessage<'D', Array<VarByteN>>;

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
