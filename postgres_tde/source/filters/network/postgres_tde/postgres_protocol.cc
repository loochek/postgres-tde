#include "postgres_tde/source/filters/network/postgres_tde/postgres_protocol.h"

namespace Envoy {
namespace Extensions {
namespace NetworkFilters {
namespace PostgresTDE {

std::unique_ptr<ReadyForQueryMessage> createReadyForQueryMessage() {
  return std::make_unique<ReadyForQueryMessage>(Byte1('I'));
}

std::unique_ptr<ErrorResponseMessage> createErrorResponseMessage(std::string error) {
  std::vector<std::unique_ptr<Sequence<Byte1, String>>> errorBody;
  errorBody.emplace_back(std::make_unique<Sequence<Byte1, String>>(Byte1('S'), String("ERROR")));
  errorBody.emplace_back(
      std::make_unique<Sequence<Byte1, String>>(Byte1('M'), String(std::move(error))));
  return std::make_unique<ErrorResponseMessage>(Repeated<Sequence<Byte1, String>>(std::move(errorBody)),
                                                Byte1('\0'));
}

} // namespace PostgresTDE
} // namespace NetworkFilters
} // namespace Extensions
} // namespace Envoy
