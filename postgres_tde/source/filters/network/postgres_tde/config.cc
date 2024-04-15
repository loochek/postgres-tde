#include "postgres_tde/source/filters/network/postgres_tde/config.h"

namespace Envoy {
namespace Extensions {
namespace NetworkFilters {
namespace PostgresTDE {

/**
 * Config registration for the Postgres proxy filter. @see NamedNetworkFilterConfigFactory.
 */
Network::FilterFactoryCb
NetworkFilters::PostgresTDE::PostgresTDEConfigFactory::createFilterFactoryFromProtoTyped(
    const envoy::extensions::filters::network::postgres_tde::PostgresTDE& proto_config,
    Server::Configuration::FactoryContext& context) {
  ASSERT(!proto_config.stat_prefix().empty());

  PostgresFilterConfig::PostgresFilterConfigOptions config_options;
  config_options.stats_prefix_ = fmt::format("postgres.{}", proto_config.stat_prefix());
  config_options.enable_sql_parsing_ =
      PROTOBUF_GET_WRAPPED_OR_DEFAULT(proto_config, enable_sql_parsing, true);
  config_options.terminate_ssl_ = proto_config.terminate_ssl();
  config_options.upstream_ssl_ = proto_config.upstream_ssl();
  config_options.permissive_parsing_ = proto_config.permissive_parsing();

  PostgresFilterConfigSharedPtr filter_config(
      std::make_shared<PostgresFilterConfig>(config_options, context.scope()));
  return [filter_config](Network::FilterManager& filter_manager) -> void {
    filter_manager.addFilter(std::make_shared<PostgresFilter>(filter_config));
  };
}

/**
 * Static registration for the Postgres proxy filter. @see RegisterFactory.
 */
REGISTER_FACTORY(PostgresTDEConfigFactory, Server::Configuration::NamedNetworkFilterConfigFactory);

} // namespace PostgresTDE
} // namespace NetworkFilters
} // namespace Extensions
} // namespace Envoy
