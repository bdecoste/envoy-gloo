workspace(name="transformation_filter")
# use skylark for native git
load('@bazel_tools//tools/build_defs/repo:git.bzl', 'git_repository')

ENVOY_SHA = "d41d06eb614fd49f19422d9eed9235c320af9229"  # April 10, 2018 (gRPC/JSON transcoder: enable preserving route after headers modified)

http_archive(
    name = "envoy",
    strip_prefix = "envoy-" + ENVOY_SHA,
    url = "https://github.com/envoyproxy/envoy/archive/" + ENVOY_SHA + ".zip",
)

ENVOY_COMMON_SHA = "2bc9569aec056df65bb4f67f0c47be968cac6256"  # Apr 3, 2018 (Fix compilation of `SoloMetadata` (#8))

http_archive(
    name = "solo_envoy_common",
    strip_prefix = "envoy-common-" + ENVOY_COMMON_SHA,
    url = "https://github.com/solo-io/envoy-common/archive/" + ENVOY_COMMON_SHA + ".zip",
)

JSON_SHA = "c8ea63a31bbcf652d61490b0ccd86771538f8c6b"

new_http_archive(
    name = "json",
    strip_prefix = "json-" + JSON_SHA + "/single_include/nlohmann",
    url = "https://github.com/nlohmann/json/archive/" + JSON_SHA + ".zip",
    build_file_content = """
cc_library(
    name = "json-lib",
    hdrs = ["json.hpp"],
    visibility = ["//visibility:public"],
)
    """
)

INJA_SHA = "74ad4281edd4ceca658888602af74bf2050107f0"

new_http_archive(
    name = "inja",
    strip_prefix = "inja-" + INJA_SHA + "/src",
    url = "https://github.com/pantor/inja/archive/" + INJA_SHA + ".zip",
    build_file_content = """
cc_library(
    name = "inja-lib",
    hdrs = ["inja.hpp"],
    visibility = ["//visibility:public"],
)
    """
)

load("@envoy//bazel:repositories.bzl", "envoy_dependencies")
load("@envoy//bazel:cc_configure.bzl", "cc_configure")

envoy_dependencies()

cc_configure()

load("@envoy_api//bazel:repositories.bzl", "api_dependencies")
api_dependencies()

load("@io_bazel_rules_go//go:def.bzl", "go_rules_dependencies", "go_register_toolchains")
load("@com_lyft_protoc_gen_validate//bazel:go_proto_library.bzl", "go_proto_repositories")
go_proto_repositories(shared=0)
go_rules_dependencies()
go_register_toolchains()
load("@io_bazel_rules_go//proto:def.bzl", "proto_register_toolchains")
proto_register_toolchains()