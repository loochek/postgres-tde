load(
    "@envoy//bazel:envoy_build_system.bzl",
    "envoy_cc_library",
)

# licenses(["notice"])  # Apache 2

package(default_visibility = ["//visibility:public"])

# PostgresSQL proxy L7 network filter.
# Public docs: https://envoyproxy.io/docs/envoy/latest/configuration/listeners/network_filters/postgres_proxy_filter

envoy_cc_library(
    name = "postgres_tde_lib",
    srcs = [
        "postgres_decoder.cc",
        "postgres_filter.cc",
        "postgres_message.cc",
        "postgres_mutation_manager.cc",
        "postgres_mutator.cc",
    ],
    hdrs = [
        "postgres_decoder.h",
        "postgres_filter.h",
        "postgres_message.h",
        "postgres_session.h",
        "postgres_mutation_manager.h",
        "postgres_mutator.h",
        "common.h",
    ],
    repository = "@envoy",
    deps = [
        "//postgres_tde/api/filters/network/postgres_tde:pkg_cc_proto",
        "//postgres_tde/source/common/sqlutils:sqlutils_lib_2",
        "@envoy//envoy/network:filter_interface",
        "@envoy//envoy/server:filter_config_interface",
        "@envoy//envoy/stats:stats_interface",
        "@envoy//envoy/stats:stats_macros",
        "@envoy//source/common/buffer:buffer_lib",
        "@envoy//source/common/network:filter_lib",
    ],
)

envoy_cc_library(
    name = "postgres_tde_config",
    srcs = ["config.cc"],
    hdrs = ["config.h", "common.h"],
    repository = "@envoy",
    deps = [
        ":postgres_tde_lib",
        "@envoy//source/extensions/filters/network/common:factory_base_lib",
        "//postgres_tde/api/filters/network/postgres_tde:pkg_cc_proto",
    ],
)
