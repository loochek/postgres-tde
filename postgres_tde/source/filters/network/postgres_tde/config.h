#pragma once

#include "source/extensions/filters/network/common/factory_base.h"
#include "source/extensions/filters/network/well_known_names.h"

#include "postgres_tde/api/filters/network/postgres_tde/postgres_tde.pb.h"
#include "postgres_tde/api/filters/network/postgres_tde/postgres_tde.pb.validate.h"
#include "postgres_tde/source/filters/network/postgres_tde/postgres_filter.h"

namespace Envoy {
namespace Extensions {
namespace NetworkFilters {
namespace PostgresTDE {

/**
 * Config registration for the Postgres proxy filter.
 */
class PostgresTDEConfigFactory
    : public Common::FactoryBase<
          envoy::extensions::filters::network::postgres_tde::PostgresTDE> {
public:
  PostgresTDEConfigFactory() : FactoryBase{NetworkFilterNames::get().PostgresProxy} {}

private:
  Network::FilterFactoryCb createFilterFactoryFromProtoTyped(
      const envoy::extensions::filters::network::postgres_tde::PostgresTDE&
          proto_config,
      Server::Configuration::FactoryContext& context) override;
};

} // namespace PostgresTDE
} // namespace NetworkFilters
} // namespace Extensions
} // namespace Envoy
