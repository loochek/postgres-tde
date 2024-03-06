load(
    "@envoy//bazel:envoy_build_system.bzl",
    "envoy_cc_library",
)

# licenses(["notice"])  # Apache 2

package(default_visibility = ["//visibility:public"])

envoy_cc_library(
    name = "sqlutils_lib_2",
    srcs = [
        "sqlutils.cc",
        "ast/visitor.cc",
    ],
    hdrs = [
        "sqlutils.h",
        "ast/visitor.h",
    ],
    external_deps = ["sqlparser"],
    deps = [
        "@envoy//source/common/protobuf:utility_lib",
        "//postgres_tde/source/common/utils:utils_lib",
    ],
)
