load(
    "//bazel:envoy_build_system.bzl",
    "envoy_cc_test",
    "envoy_cc_test_library",
    "envoy_contrib_package",
    "envoy_proto_library",
)

licenses(["notice"])  # Apache 2

envoy_contrib_package()

envoy_cc_test_library(
    name = "postgres_test_utils_lib",
    srcs = ["postgres_test_utils.cc"],
    hdrs = ["postgres_test_utils.h"],
    deps = [
        "//postgres_tde/source/filters/network/postgres_tde:filter",
        "//source/common/buffer:buffer_lib",
    ],
)

envoy_cc_test(
    name = "postgres_decoder_tests",
    srcs = [
        "postgres_decoder_test.cc",
    ],
    deps = [
        ":postgres_test_utils_lib",
        "//postgres_tde/source/filters/network/postgres_tde:filter",
        "//test/mocks/network:network_mocks",
    ],
)

envoy_cc_test(
    name = "postgres_message_tests",
    srcs = [
        "postgres_message_test.cc",
    ],
    deps = [
        "//postgres_tde/source/filters/network/postgres_tde:filter",
        "//source/common/buffer:buffer_lib",
    ],
)

envoy_cc_test(
    name = "postgres_filter_tests",
    srcs = [
        "postgres_filter_test.cc",
    ],
    deps = [
        ":postgres_test_utils_lib",
        "//postgres_tde/source/filters/network/postgres_tde:filter",
        "//test/mocks/network:network_mocks",
    ],
)

envoy_proto_library(
    name = "postgres_integration_proto",
    srcs = [":postgres_integration_test.proto"],
)

envoy_cc_test(
    name = "postgres_integration_test",
    srcs = [
        "postgres_integration_test.cc",
    ],
    data = [
        "postgres_test_config.yaml-template",
        "//test/config/integration/certs",
    ],
    deps = [
        ":postgres_integration_proto_cc_proto",
        ":postgres_test_utils_lib",
        "//postgres_tde/source/filters/network/postgres_tde:config",
        "//postgres_tde/source/filters/network/postgres_tde:filter",
        "//source/common/tcp_proxy",
        "//source/extensions/filters/network/tcp_proxy:config",
        "//source/extensions/transport_sockets/starttls:config",
        "//test/integration:integration_lib",
        "//test/test_common:registry_lib",
        "@envoy_api//contrib/envoy/extensions/filters/network/postgres_proxy/v3alpha:pkg_cc_proto",
    ],
)
